import asyncio
from dataclasses import dataclass
import fnmatch
import json
import os
import shlex
import sys
import time
from pathlib import Path

import fitz  # pymupdf
import httpx
from duckduckgo_search import DDGS
from mcp import ClientSession
from mcp.client.stdio import StdioServerParameters, stdio_client
from mcp.server.fastmcp import FastMCP

PROJECT_ROOT = Path(os.getcwd()).resolve()


def _load_env_file() -> None:
    env_path = PROJECT_ROOT / ".env"
    if not env_path.is_file():
        return

    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip("'\"")
        if key:
            os.environ.setdefault(key, value)


_load_env_file()

BASE_URL = os.getenv("LLAMA_BASE_URL", "http://127.0.0.1:8080/v1")
CONDA_BIN = Path(sys.executable).parent  # e.g. .../envs/codex/bin
CONTEXT7_COMMAND = os.getenv("CONTEXT7_COMMAND", "")
CONTEXT7_ARGS = os.getenv("CONTEXT7_ARGS", "")
CONTEXT7_NODE = os.getenv("CONTEXT7_NODE", "")
CONTEXT7_SCRIPT = os.getenv("CONTEXT7_SCRIPT", "")
SLOT_IDS = (0, 1)
EXCERPT_SLOT_ID = 0
CODE_SLOT_ID = 1
SLOT_PURPOSES = {
    EXCERPT_SLOT_ID: "excerpt_and_plan_document_updates",
    CODE_SLOT_ID: "code_generation_and_editing",
}


def _env_bool(name: str, default: bool) -> bool:
    raw_value = os.getenv(name)
    if raw_value is None:
        return default
    return raw_value.strip().lower() not in {"0", "false", "no", "off", ""}


def _env_float(name: str, default: float) -> float:
    raw_value = os.getenv(name)
    if raw_value is None:
        return default
    try:
        return float(raw_value)
    except ValueError:
        return default


def _env_int(name: str, default: int) -> int:
    raw_value = os.getenv(name)
    if raw_value is None:
        return default
    try:
        return int(raw_value)
    except ValueError:
        return default


CONTEXT_CLEANUP_ENABLED = _env_bool("LLAMA_CONTEXT_CLEANUP_ENABLED", True)
CONTEXT_MAX_RATIO = _env_float("LLAMA_CONTEXT_MAX_RATIO", 0.85)
CONTEXT_IDLE_SECONDS = _env_float("LLAMA_CONTEXT_IDLE_SECONDS", 15 * 60)
CONTEXT_CHECK_INTERVAL_SECONDS = _env_float("LLAMA_CONTEXT_CHECK_INTERVAL_SECONDS", 60)
CONTEXT_FALLBACK_N_CTX = _env_int("LLAMA_CONTEXT_FALLBACK_N_CTX", 0)
CONTEXT_TOKEN_CHARS = max(1, _env_int("LLAMA_CONTEXT_TOKEN_CHARS", 4))
CONTEXT_CLEAR_AFTER_CALL = _env_bool("LLAMA_CONTEXT_CLEAR_AFTER_CALL", True)
CONTEXT_MEMORY_ENABLED = _env_bool("LLAMA_CONTEXT_MEMORY_ENABLED", True)
CONTEXT_MEMORY_DIR = os.getenv("LLAMA_CONTEXT_MEMORY_DIR", ".llama/llama-context")
CONTEXT_MEMORY_MAX_CHARS = max(1_000, _env_int("LLAMA_CONTEXT_MEMORY_MAX_CHARS", 200_000))
CONTEXT_MEMORY_SEARCH_RESULTS = max(1, _env_int("LLAMA_CONTEXT_MEMORY_SEARCH_RESULTS", 8))
CONTEXT_DUMP_FINAL_AFTER_CALL = _env_bool("LLAMA_CONTEXT_DUMP_FINAL_AFTER_CALL", True)
CONTEXT_DUMP_FINAL_MAX_CHARS = max(
    1_000,
    _env_int("LLAMA_CONTEXT_DUMP_FINAL_MAX_CHARS", 50_000),
)


@dataclass
class SlotContextState:
    last_used_at: float = 0.0
    last_checked_at: float = 0.0
    last_total_tokens: int = 0
    n_ctx: int | None = None
    reset_needed: bool = False
    last_cleanup_at: float = 0.0
    last_cleanup_reason: str = ""
    last_cleanup_error: str = ""


_SLOT_CONTEXTS = {slot_id: SlotContextState() for slot_id in SLOT_IDS}
_SLOT_LOCKS = {slot_id: asyncio.Lock() for slot_id in SLOT_IDS}
_CONTEXT_CLEANUP_TASK: asyncio.Task | None = None


def _load_deny_patterns() -> list[str]:
    base_patterns = [
        ".env",
        ".env.*",
        "**/.env",
        "**/.env.*",
        ".env/**",
        ".env.*/**",
        "**/.env/**",
        "**/.env.*/**",
        "*.pem",
        "**/*.pem",
        "*.key",
        "**/*.key",
        "*_rsa",
        "**/*_rsa",
        "id_rsa",
        "**/id_rsa",
        "id_ed25519",
        "**/id_ed25519",
        "*.p12",
        "**/*.p12",
        "*.pfx",
        "**/*.pfx",
        "*secret*",
        "**/*secret*",
        "*credential*",
        "**/*credential*",
    ]
    settings_path = PROJECT_ROOT / ".claude" / "settings.json"
    try:
        settings = json.loads(settings_path.read_text(encoding="utf-8"))
    except Exception:
        return base_patterns

    permission_block = settings.get("permissions", {})
    deny = permission_block.get("deny", [])
    if not isinstance(deny, list):
        return base_patterns

    patterns = base_patterns[:]
    for entry in deny:
        if not isinstance(entry, str):
            continue
        pattern = entry.strip()
        if not pattern:
            continue
        if "(" in pattern and pattern.endswith(")"):
            pattern = pattern.split("(", 1)[1][:-1].strip()
        patterns.append(pattern)
        if pattern.startswith("./"):
            patterns.append(pattern[2:])
    return patterns


DENY_PATTERNS = _load_deny_patterns()


def _check_path(path_str: str) -> Path:
    raw_path = Path(path_str)
    path = raw_path if raw_path.is_absolute() else PROJECT_ROOT / raw_path
    path = path.resolve()
    if not path.is_relative_to(PROJECT_ROOT):
        raise PermissionError(f"path outside project root: {path_str}")

    rel_path = path.relative_to(PROJECT_ROOT).as_posix()
    names_to_check = {rel_path, path.name}
    for pattern in DENY_PATTERNS:
        normalized = pattern.replace("\\", "/")
        if any(fnmatch.fnmatch(name, normalized) for name in names_to_check):
            raise PermissionError(f"path denied by pattern {pattern!r}: {path_str}")
    return path


def _context_memory_root() -> Path:
    path = _check_path(CONTEXT_MEMORY_DIR)
    path.mkdir(parents=True, exist_ok=True)
    return path


def _slugify(value: str) -> str:
    chars = []
    previous_dash = False
    for char in value.lower():
        if char.isalnum():
            chars.append(char)
            previous_dash = False
        elif not previous_dash:
            chars.append("-")
            previous_dash = True
    slug = "".join(chars).strip("-")
    return slug[:60] or "context"


def _context_doc_path(path_str: str) -> Path:
    root = _context_memory_root()
    raw_path = Path(path_str)
    if raw_path.is_absolute():
        path = raw_path.resolve()
    else:
        project_relative_path = (PROJECT_ROOT / raw_path).resolve()
        if project_relative_path.is_relative_to(root):
            path = project_relative_path
        else:
            path = (root / raw_path).resolve()
    if not path.is_relative_to(root):
        raise PermissionError(f"context document outside memory dir: {path_str}")
    if path.suffix != ".md":
        raise PermissionError(f"context document must be a Markdown file: {path_str}")
    return path


def _normalize_tags(tags: object) -> list[str]:
    if isinstance(tags, str):
        raw_tags = [tag.strip() for tag in tags.split(",")]
    elif isinstance(tags, list):
        raw_tags = [str(tag).strip() for tag in tags]
    else:
        raw_tags = []
    return [tag for tag in raw_tags if tag]


def _write_context_memory(title: str, content: str, tags: object = None) -> str:
    if not CONTEXT_MEMORY_ENABLED:
        return "Error: context memory is disabled"

    title = title.strip() or "Untitled context"
    content = content.strip()
    if not content:
        return "Error: context content is empty"
    if len(content) > CONTEXT_MEMORY_MAX_CHARS:
        return (
            "Error: context content exceeds "
            f"LLAMA_CONTEXT_MEMORY_MAX_CHARS={CONTEXT_MEMORY_MAX_CHARS}"
        )

    root = _context_memory_root()
    timestamp = time.strftime("%Y%m%d-%H%M%S", time.localtime())
    path = root / f"{timestamp}-{_slugify(title)}.md"
    tag_list = _normalize_tags(tags)
    tag_line = ", ".join(tag_list) if tag_list else "-"
    body = (
        f"# {title}\n\n"
        f"Created: {time.strftime('%Y-%m-%d %H:%M:%S %z', time.localtime())}\n"
        f"Tags: {tag_line}\n\n"
        "---\n\n"
        f"{content}\n"
    )
    path.write_text(body, encoding="utf-8")
    return path.relative_to(PROJECT_ROOT).as_posix()


_SKILLS_ROOT = PROJECT_ROOT / ".llama" / "skills"


def _skills_root() -> Path:
    _SKILLS_ROOT.mkdir(parents=True, exist_ok=True)
    return _SKILLS_ROOT


def _skill_filename(source: str, key_args: dict) -> str:
    key = source + "__" + "__".join(f"{k}_{v}" for k, v in sorted(key_args.items()))
    return _slugify(key) + ".md"


def _parse_skill_frontmatter(text: str) -> dict:
    meta: dict = {}
    if not text.startswith("---"):
        return meta
    end = text.find("\n---", 3)
    if end == -1:
        return meta
    for line in text[3:end].splitlines():
        if ":" in line:
            k, _, v = line.partition(":")
            meta[k.strip()] = v.strip()
    return meta


def _skill_is_fresh(meta: dict, source: str) -> bool:
    from datetime import date

    if source == "read_pdf":
        stored = meta.get("source_mtime")
        pdf_path = meta.get("path", "")
        if not stored:
            return False
        try:
            actual = os.path.getmtime(str(PROJECT_ROOT / pdf_path))
            return abs(actual - float(stored)) < 1.0
        except OSError:
            return False
    expires_str = meta.get("expires", "")
    if not expires_str:
        return False
    try:
        return date.today() <= date.fromisoformat(expires_str)
    except ValueError:
        return False


def _write_skill(
    source: str,
    key_args: dict,
    content: str,
    ttl_days: int = 7,
    source_mtime: float | None = None,
) -> None:
    from datetime import date, timedelta

    skill_path = _skills_root() / _skill_filename(source, key_args)
    topic = " | ".join(str(v) for v in key_args.values())[:80]
    key_lines = "\n".join(f"{k}: {v}" for k, v in key_args.items())
    if source == "read_pdf":
        expiry_line = f"source_mtime: {source_mtime}"
    else:
        expires = date.today() + timedelta(days=ttl_days)
        expiry_line = f"expires: {expires.isoformat()}"
    doc = (
        f"---\nsource: {source}\ntopic: {topic}\n{key_lines}\n"
        f"created: {date.today().isoformat()}\n{expiry_line}\n---\n\n{content}"
    )
    skill_path.write_text(doc, encoding="utf-8")


def _list_skills(query: str = "") -> list[dict]:
    root = _skills_root()
    results = []
    query_lower = query.lower()
    for p in sorted(root.glob("*.md"), key=lambda x: x.stat().st_mtime, reverse=True):
        try:
            text = p.read_text(encoding="utf-8")
            meta = _parse_skill_frontmatter(text)
            source = meta.get("source", "")
            topic = meta.get("topic", p.stem)
            if query_lower and query_lower not in topic.lower() and query_lower not in text.lower():
                continue
            results.append(
                {
                    "path": str(p.relative_to(PROJECT_ROOT)),
                    "topic": topic,
                    "source": source,
                    "created": meta.get("created", ""),
                    "fresh": _skill_is_fresh(meta, source),
                }
            )
        except Exception:
            continue
    return results


def _list_context_memories(limit: int = 20) -> list[dict]:
    if not CONTEXT_MEMORY_ENABLED:
        return []

    root = _context_memory_root()
    docs = sorted(root.glob("*.md"), key=lambda path: path.stat().st_mtime, reverse=True)
    results = []
    for path in docs[: max(1, limit)]:
        text = path.read_text(encoding="utf-8", errors="replace")
        title = path.stem
        for line in text.splitlines():
            if line.startswith("# "):
                title = line[2:].strip()
                break
        results.append(
            {
                "path": path.relative_to(PROJECT_ROOT).as_posix(),
                "title": title,
                "size": path.stat().st_size,
                "modified": int(path.stat().st_mtime),
            }
        )
    return results


def _search_context_memories(query: str, max_results: int | None = None) -> list[dict]:
    if not CONTEXT_MEMORY_ENABLED:
        return []

    query = query.strip()
    if not query:
        return _list_context_memories(max_results or CONTEXT_MEMORY_SEARCH_RESULTS)

    terms = [term.lower() for term in query.split() if term.strip()]
    root = _context_memory_root()
    scored = []
    for path in root.glob("*.md"):
        text = path.read_text(encoding="utf-8", errors="replace")
        lower_text = text.lower()
        score = sum(lower_text.count(term) for term in terms)
        if score <= 0:
            continue
        first_index = min(
            (lower_text.find(term) for term in terms if lower_text.find(term) >= 0),
            default=0,
        )
        start = max(0, first_index - 160)
        end = min(len(text), first_index + 360)
        excerpt = text[start:end].strip()
        scored.append((score, path.stat().st_mtime, path, excerpt))

    scored.sort(key=lambda item: (item[0], item[1]), reverse=True)
    results = []
    limit = max_results or CONTEXT_MEMORY_SEARCH_RESULTS
    for score, _, path, excerpt in scored[: max(1, limit)]:
        results.append(
            {
                "path": path.relative_to(PROJECT_ROOT).as_posix(),
                "score": score,
                "excerpt": excerpt,
            }
        )
    return results


def _read_context_memory(path_str: str) -> str:
    if not CONTEXT_MEMORY_ENABLED:
        return "Error: context memory is disabled"

    path = _context_doc_path(path_str)
    if not path.is_file():
        return f"Error: context document not found: {path_str}"
    return path.read_text(encoding="utf-8", errors="replace")


def _dump_final_context(slot_id: int, content: str) -> str:
    if not CONTEXT_DUMP_FINAL_AFTER_CALL or not content.strip():
        return ""

    final_content = content.strip()
    if len(final_content) > CONTEXT_DUMP_FINAL_MAX_CHARS:
        omitted = len(final_content) - CONTEXT_DUMP_FINAL_MAX_CHARS
        final_content = (
            final_content[:CONTEXT_DUMP_FINAL_MAX_CHARS]
            + f"\n\n[truncated {omitted} chars by LLAMA_CONTEXT_DUMP_FINAL_MAX_CHARS]"
        )

    return _write_context_memory(
        f"Local LLM work unit result slot {slot_id}",
        final_content,
        ["auto-dump", f"slot-{slot_id}"],
    )


def _validate_slot_id(slot_id: int) -> str | None:
    if slot_id not in SLOT_IDS:
        return f"Error: invalid slot_id {slot_id!r}"
    return None


def _llama_root_url() -> str:
    return BASE_URL.removesuffix("/v1").rstrip("/")


def _slot_url(slot_id: int) -> str:
    return f"{_llama_root_url()}/slots/{slot_id}"


def _cleanup_interval() -> float:
    return max(5.0, CONTEXT_CHECK_INTERVAL_SECONDS)


def _estimate_message_tokens(messages: list[dict]) -> int:
    serialized = json.dumps(messages, ensure_ascii=False, separators=(",", ":"))
    return max(1, len(serialized) // CONTEXT_TOKEN_CHARS)


def _extract_int(value: object) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)
    return None


def _extract_usage_tokens(usage: object) -> int | None:
    if not isinstance(usage, dict):
        return None

    total = _extract_int(usage.get("total_tokens"))
    if total is not None:
        return total

    prompt = _extract_int(usage.get("prompt_tokens")) or 0
    completion = _extract_int(usage.get("completion_tokens")) or 0
    total = prompt + completion
    if total > 0:
        return total

    prompt = _extract_int(usage.get("prompt_n")) or 0
    cache = _extract_int(usage.get("cache_n")) or 0
    predicted = _extract_int(usage.get("predicted_n")) or 0
    total = prompt + cache + predicted
    if total > 0:
        return total
    return None


def _extract_slot_tokens(slot: dict) -> int | None:
    direct_fields = (
        "n_past",
        "n_tokens",
        "n_prompt_tokens",
        "prompt_tokens",
        "total_tokens",
    )
    for field in direct_fields:
        value = _extract_int(slot.get(field))
        if value is not None and value >= 0:
            return value

    additive_fields = ("prompt_n", "cache_n", "predicted_n")
    values = [_extract_int(slot.get(field)) for field in additive_fields]
    if any(value is not None for value in values):
        return sum(value or 0 for value in values)
    return None


def _threshold_tokens(state: SlotContextState) -> int | None:
    if CONTEXT_MAX_RATIO <= 0:
        return None
    n_ctx = state.n_ctx or CONTEXT_FALLBACK_N_CTX
    if n_ctx <= 0:
        return None
    return max(1, int(n_ctx * CONTEXT_MAX_RATIO))


def _cleanup_state(slot_id: int, reason: str, error: str = "") -> None:
    state = _SLOT_CONTEXTS[slot_id]
    state.last_cleanup_at = time.monotonic()
    state.last_cleanup_reason = reason
    state.last_cleanup_error = error
    if not error:
        state.last_total_tokens = 0
        state.reset_needed = False
        state.last_used_at = 0.0


async def _fetch_slot(client: httpx.AsyncClient, slot_id: int) -> dict | None:
    if CONTEXT_CHECK_INTERVAL_SECONDS <= 0:
        return None

    response = await client.get(f"{_llama_root_url()}/slots", timeout=10.0)
    if response.status_code in (403, 404, 501):
        return None
    response.raise_for_status()
    payload = response.json()

    if isinstance(payload, dict):
        slots = payload.get("slots")
        if slots is None and payload.get("id") == slot_id:
            return payload
    else:
        slots = payload

    if not isinstance(slots, list):
        return None
    for slot in slots:
        if isinstance(slot, dict) and slot.get("id") == slot_id:
            return slot
    return None


async def _refresh_slot_state(
    client: httpx.AsyncClient,
    slot_id: int,
    force: bool = False,
) -> None:
    if not CONTEXT_CLEANUP_ENABLED:
        return

    state = _SLOT_CONTEXTS[slot_id]
    now = time.monotonic()
    if not force and now - state.last_checked_at < _cleanup_interval():
        return

    state.last_checked_at = now
    try:
        slot = await _fetch_slot(client, slot_id)
    except Exception as e:
        state.last_cleanup_error = f"slot status unavailable: {e}"
        return

    if slot is None:
        return

    n_ctx = _extract_int(slot.get("n_ctx"))
    if n_ctx is not None and n_ctx > 0:
        state.n_ctx = n_ctx

    slot_tokens = _extract_slot_tokens(slot)
    if slot_tokens is not None:
        state.last_total_tokens = max(state.last_total_tokens, slot_tokens)

    threshold = _threshold_tokens(state)
    if threshold is not None and state.last_total_tokens >= threshold:
        state.reset_needed = True


async def _erase_slot(
    client: httpx.AsyncClient,
    slot_id: int,
    reason: str,
    force: bool = False,
) -> bool:
    if not force and not CONTEXT_CLEANUP_ENABLED:
        return False

    async with _SLOT_LOCKS[slot_id]:
        try:
            response = await client.post(
                _slot_url(slot_id),
                params={"action": "erase"},
                timeout=10.0,
            )
            if response.status_code in (403, 404, 501):
                _cleanup_state(
                    slot_id,
                    reason,
                    f"slot erase unavailable: HTTP {response.status_code}",
                )
                return False
            response.raise_for_status()
        except Exception as e:
            _cleanup_state(slot_id, reason, f"slot erase failed: {e}")
            return False

        _cleanup_state(slot_id, reason)
        return True


async def _maybe_cleanup_slot(
    client: httpx.AsyncClient,
    slot_id: int,
    estimated_request_tokens: int = 0,
) -> None:
    if not CONTEXT_CLEANUP_ENABLED:
        return

    await _refresh_slot_state(client, slot_id)

    state = _SLOT_CONTEXTS[slot_id]
    now = time.monotonic()
    if (
        CONTEXT_IDLE_SECONDS > 0
        and state.last_used_at > 0
        and now - state.last_used_at >= CONTEXT_IDLE_SECONDS
    ):
        await _erase_slot(client, slot_id, f"idle for {int(now - state.last_used_at)}s")
        return

    threshold = _threshold_tokens(state)
    observed_tokens = max(state.last_total_tokens, estimated_request_tokens)
    if threshold is not None and observed_tokens >= threshold:
        state.reset_needed = True

    if state.reset_needed:
        await _erase_slot(client, slot_id, "context threshold reached")


def _mark_slot_used(slot_id: int, usage: object, fallback_tokens: int = 0) -> None:
    if not CONTEXT_CLEANUP_ENABLED:
        return

    state = _SLOT_CONTEXTS[slot_id]
    state.last_used_at = time.monotonic()
    total_tokens = _extract_usage_tokens(usage) or fallback_tokens
    if total_tokens > 0:
        state.last_total_tokens = max(state.last_total_tokens, total_tokens)

    threshold = _threshold_tokens(state)
    if threshold is not None and state.last_total_tokens >= threshold:
        state.reset_needed = True


async def _context_cleanup_loop() -> None:
    while True:
        await asyncio.sleep(_cleanup_interval())
        if not CONTEXT_CLEANUP_ENABLED or CONTEXT_IDLE_SECONDS <= 0:
            continue

        async with httpx.AsyncClient(timeout=None) as client:
            for slot_id, state in _SLOT_CONTEXTS.items():
                now = time.monotonic()
                if state.last_used_at <= 0:
                    continue
                if now - state.last_used_at < CONTEXT_IDLE_SECONDS:
                    continue
                await _erase_slot(client, slot_id, f"idle for {int(now - state.last_used_at)}s")


def _ensure_context_cleanup_task() -> None:
    global _CONTEXT_CLEANUP_TASK
    if not CONTEXT_CLEANUP_ENABLED or CONTEXT_IDLE_SECONDS <= 0:
        return
    if _CONTEXT_CLEANUP_TASK is not None and not _CONTEXT_CLEANUP_TASK.done():
        return
    try:
        loop = asyncio.get_running_loop()
    except RuntimeError:
        return
    _CONTEXT_CLEANUP_TASK = loop.create_task(_context_cleanup_loop())


def _slot_status_payload(slot_id: int | None = None) -> dict:
    selected_slots = SLOT_IDS if slot_id is None else (slot_id,)
    now = time.monotonic()
    slots = {}
    for selected_slot_id in selected_slots:
        state = _SLOT_CONTEXTS[selected_slot_id]
        threshold = _threshold_tokens(state)
        slots[str(selected_slot_id)] = {
            "purpose": SLOT_PURPOSES.get(selected_slot_id, "unspecified"),
            "last_used_seconds_ago": (
                None if state.last_used_at <= 0 else round(now - state.last_used_at, 3)
            ),
            "last_total_tokens": state.last_total_tokens,
            "n_ctx": state.n_ctx,
            "threshold_tokens": threshold,
            "reset_needed": state.reset_needed,
            "last_cleanup_seconds_ago": (
                None if state.last_cleanup_at <= 0 else round(now - state.last_cleanup_at, 3)
            ),
            "last_cleanup_reason": state.last_cleanup_reason,
            "last_cleanup_error": state.last_cleanup_error,
        }

    return {
        "enabled": CONTEXT_CLEANUP_ENABLED,
        "idle_seconds": CONTEXT_IDLE_SECONDS,
        "max_ratio": CONTEXT_MAX_RATIO,
        "check_interval_seconds": CONTEXT_CHECK_INTERVAL_SECONDS,
        "fallback_n_ctx": CONTEXT_FALLBACK_N_CTX,
        "clear_after_call": CONTEXT_CLEAR_AFTER_CALL,
        "memory_enabled": CONTEXT_MEMORY_ENABLED,
        "memory_dir": _context_memory_root().relative_to(PROJECT_ROOT).as_posix()
        if CONTEXT_MEMORY_ENABLED
        else None,
        "dump_final_after_call": CONTEXT_DUMP_FINAL_AFTER_CALL,
        "dump_final_max_chars": CONTEXT_DUMP_FINAL_MAX_CHARS,
        "slots": slots,
    }


def _read_prompt_file(rule_path: str) -> str:
    path = PROJECT_ROOT / rule_path
    try:
        return path.read_text(encoding="utf-8").strip()
    except Exception:
        return ""


def _load_role_prompt(role: str, prompt_scope: str = "") -> str:
    rule_paths = [
        *PROMPT_SCOPE_RULE_FILES.get(prompt_scope, []),
        *ROLE_REQUIRED_RULE_FILES.get(role, []),
        ROLE_RULE_FILES.get(role, ROLE_RULE_FILES["general"]),
    ]
    prompts = []
    for rule_path in rule_paths:
        prompt = _read_prompt_file(rule_path)
        if prompt:
            prompts.append(f"<!-- {rule_path} -->\n{prompt}")

    if prompts:
        return "\n\n".join(prompts)

    fallback = _read_prompt_file(ROLE_RULE_FILES["general"])
    if fallback:
        return fallback
    return "Act as a local teammate for one small work unit. Keep scope tight."


def _with_system_prompts(
    messages: list[dict],
    use_tools: bool,
    role: str = "general",
    prompt_scope: str = "",
) -> list[dict]:
    conversation = [dict(message) for message in messages]
    prompts = []
    role_prompt = _load_role_prompt(role, prompt_scope)
    if role_prompt:
        prompts.append(role_prompt)
    if use_tools and CONTEXT_MEMORY_ENABLED:
        prompts.append(CONTEXT_MANAGEMENT_PROMPT)
    if not prompts:
        return conversation

    prompt = "\n\n".join(prompts)
    if conversation and conversation[0].get("role") == "system":
        existing_content = str(conversation[0].get("content", ""))
        conversation[0]["content"] = f"{existing_content}\n\n{prompt}"
    else:
        conversation.insert(0, {"role": "system", "content": prompt})
    return conversation


async def _finalize_context(
    client: httpx.AsyncClient,
    slot_id: int,
    reason: str = "work unit completed",
    final_content: str = "",
) -> None:
    if CONTEXT_MEMORY_ENABLED and CONTEXT_DUMP_FINAL_AFTER_CALL:
        try:
            _dump_final_context(slot_id, final_content)
        except Exception:
            pass

    if not CONTEXT_CLEAR_AFTER_CALL:
        return
    await _erase_slot(client, slot_id, reason)


async def _call_context7(tool_name: str, args: dict) -> str:
    if CONTEXT7_COMMAND:
        command = CONTEXT7_COMMAND
        command_args = shlex.split(CONTEXT7_ARGS)
    elif CONTEXT7_NODE and CONTEXT7_SCRIPT:
        command = CONTEXT7_NODE
        command_args = [CONTEXT7_SCRIPT]
    else:
        command = "npx"
        command_args = ["-y", "@upstash/context7-mcp"]

    server_params = StdioServerParameters(
        command=command,
        args=command_args,
    )
    async with stdio_client(server_params) as streams:
        async with ClientSession(*streams) as session:
            await session.initialize()
            result = await session.call_tool(tool_name, args)

    parts = []
    for item in result.content:
        text = getattr(item, "text", None)
        if text is not None:
            parts.append(str(text))
            continue
        structured = getattr(item, "structuredContent", None)
        if structured is not None:
            parts.append(json.dumps(structured, ensure_ascii=False))
            continue
        if hasattr(item, "model_dump"):
            parts.append(json.dumps(item.model_dump(mode="json"), ensure_ascii=False))
        else:
            parts.append(str(item))

    if parts:
        return "\n".join(parts)
    if result.structuredContent is not None:
        return json.dumps(result.structuredContent, ensure_ascii=False)
    return ""


CONTEXT7_TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "context7_resolve_library",
            "description": "Resolve a library name to a Context7 library ID using up-to-date docs metadata.",
            "parameters": {
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "What documentation or capability you want to find.",
                    },
                    "libraryName": {
                        "type": "string",
                        "description": "Library or package name to resolve.",
                    },
                },
                "required": ["query", "libraryName"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "context7_query_docs",
            "description": "Query Context7 for up-to-date library documentation and examples.",
            "parameters": {
                "type": "object",
                "properties": {
                    "libraryId": {
                        "type": "string",
                        "description": "Resolved Context7 library ID such as /org/project.",
                    },
                    "query": {
                        "type": "string",
                        "description": "Specific documentation question to ask.",
                    },
                },
                "required": ["libraryId", "query"],
            },
        },
    },
]


CONTEXT_MANAGEMENT_PROMPT = (
    "You are running on a small local LLM context. Treat each requested task as a "
    "small work unit. Before the final answer, save exact facts that must survive "
    "slot cleanup with remember_context: decisions, constraints, file paths, command "
    "outputs, API details, and code snippets that should not be lossy-summarized. "
    "The final answer is saved automatically, but exact intermediate facts still "
    "need remember_context when they matter. "
    "At the start of a task, use search_context or read_context_doc when previous "
    "local context may be relevant. Do not store secrets, credentials, private keys, "
    "tokens, or .env contents.\n"
    "Knowledge cutoff: your training data covers up to approximately early 2025 "
    "(Qwen 3.5 base). For any library API, framework version, or factual claim "
    "where you are not certain or where the information may have changed after "
    "early 2025, you MUST use the available tools before answering: use "
    "context7_resolve_library + context7_query_docs for library/API questions, "
    "and web_search for current events, release notes, or any post-training facts. "
    "Do not guess or hallucinate version numbers, API signatures, or recent changes. "
    "Before calling web_search, context7_query_docs, or read_pdf: first call "
    "find_skill with a short relevant keyword. If the response contains a fresh=true "
    "entry, call read_file on its path and use that content - skip the live tool. "
    "If no fresh entry is found, call the live tool normally; results are saved "
    "automatically."
)

ROLE_RULE_FILES = {
    "general": ".llama/prompt/general.md",
    "teammate": ".llama/prompt/teammate.md",
    "implementation": ".llama/prompt/implementation.md",
}

ROLE_REQUIRED_RULE_FILES = {
    "implementation": [".claude/rules/Guide_Behavior.md"],
    "edit": [".claude/rules/Guide_Behavior.md"],
}

PROMPT_SCOPE_RULE_FILES = {
    "plan": [".claude/rules/Edit_Workflow.md"],
}

CONTEXT_MEMORY_TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "remember_context",
            "description": (
                "Write important exact facts or snippets to a temporary Markdown "
                "context document before the current local LLM slot is cleared. "
                "Do not store secrets, credentials, private keys, tokens, or .env contents."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "title": {
                        "type": "string",
                        "description": "Short title for the context document.",
                    },
                    "content": {
                        "type": "string",
                        "description": "Exact facts, snippets, paths, or command output to preserve.",
                    },
                    "tags": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "Optional search tags.",
                    },
                },
                "required": ["title", "content"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "search_context",
            "description": "Search temporary local LLM context documents by keyword.",
            "parameters": {
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "Keywords to search for.",
                    },
                    "max_results": {
                        "type": "integer",
                        "description": "Maximum results to return.",
                    },
                },
                "required": ["query"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "read_context_doc",
            "description": "Read one temporary local LLM context Markdown document.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Path returned by remember_context or search_context.",
                    },
                },
                "required": ["path"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "list_context_docs",
            "description": "List recent temporary local LLM context documents.",
            "parameters": {
                "type": "object",
                "properties": {
                    "limit": {
                        "type": "integer",
                        "description": "Maximum documents to list.",
                    },
                },
            },
        },
    },
]


READ_TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "read_file",
            "description": "Read a UTF-8 text file inside the project.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Project-relative path to read.",
                    },
                },
                "required": ["path"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "list_dir",
            "description": "List files and directories inside the project.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Project-relative directory path to list.",
                        "default": ".",
                    },
                },
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "run_bash",
            "description": (
                "Run a shell command inside the project root with the conda environment active. "
                "Use for running python scripts, linters, tests, git commands, etc. "
                "stdout and stderr are returned combined. Timeout: 60s."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "command": {
                        "type": "string",
                        "description": "Shell command to execute.",
                    },
                },
                "required": ["command"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "web_search",
            "description": "Search the web using DuckDuckGo. Returns titles, URLs, and snippets.",
            "parameters": {
                "type": "object",
                "properties": {
                    "query": {"type": "string", "description": "Search query."},
                    "max_results": {
                        "type": "integer",
                        "description": "Max results to return (default 5, max 10).",
                    },
                },
                "required": ["query"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "read_pdf",
            "description": "Extract text from a PDF file inside the project. Returns page-by-page text.",
            "parameters": {
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Project-relative path to the PDF file.",
                    },
                    "max_chars": {
                        "type": "integer",
                        "description": "Maximum characters to return (default 8000).",
                    },
                },
                "required": ["path"],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "find_skill",
            "description": (
                "Search .llama/skills/ for cached knowledge documents from previous "
                "web_search, context7, or read_pdf calls. Returns list with path, topic, "
                "source, created date, and fresh (bool). If fresh=true, call read_file on "
                "the path to get the content - skip the live tool call. If stale or absent, "
                "call the live tool normally; results are auto-saved - do not save manually."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "query": {
                        "type": "string",
                        "description": "Keyword to filter by topic or content. Empty string lists all.",
                    },
                },
                "required": ["query"],
            },
        },
    },
] + CONTEXT7_TOOLS + CONTEXT_MEMORY_TOOLS

PROPOSE_WRITE_DEF = {
    "type": "function",
    "function": {
        "name": "propose_write",
        "description": "Queue a file write for human review. Do NOT use write_file directly.",
        "parameters": {
            "type": "object",
            "properties": {
                "path": {
                    "type": "string",
                    "description": "Project-relative path to write.",
                },
                "content": {
                    "type": "string",
                    "description": "Full file content to write.",
                },
                "reason": {
                    "type": "string",
                    "description": "Why this file change is needed.",
                },
            },
            "required": ["path", "content", "reason"],
        },
    },
}

PLAN_TOOLS = READ_TOOLS + [PROPOSE_WRITE_DEF]

PROFILES = {
    "plan": {
        "temperature": 0.1,
        "top_p": 0.9,
        "extra_body": {
            "top_k": 20,
            "cache_prompt": True,
            "chat_template_kwargs": {"enable_thinking": False},
        },
    },
    "coding": {
        "temperature": 0.6,
        "top_p": 0.95,
        "extra_body": {
            "top_k": 20,
            "cache_prompt": True,
            "chat_template_kwargs": {"enable_thinking": True},
        },
    },
    "text": {
        "temperature": 0.7,
        "top_p": 0.95,
        "presence_penalty": 0.5,
        "extra_body": {
            "top_k": 20,
            "cache_prompt": True,
            "chat_template_kwargs": {"enable_thinking": False},
        },
    },
}

mcp = FastMCP("llama")


async def _execute_tool(name: str, args: dict) -> str:
    try:
        if name == "context7_resolve_library":
            return await _call_context7(
                "resolve-library-id",
                {
                    "query": str(args["query"]),
                    "libraryName": str(args["libraryName"]),
                },
            )

        if name == "context7_query_docs":
            library_id = str(args["libraryId"])
            ctx7_query = str(args["query"])
            result = await _call_context7(
                "query-docs",
                {"libraryId": library_id, "query": ctx7_query},
            )
            try:
                _write_skill(
                    "context7",
                    {"libraryId": library_id, "query": ctx7_query},
                    result,
                    ttl_days=30,
                )
            except Exception:
                pass
            return result

        if name == "remember_context":
            return _write_context_memory(
                str(args["title"]),
                str(args["content"]),
                args.get("tags"),
            )

        if name == "search_context":
            max_results = _extract_int(args.get("max_results"))
            results = _search_context_memories(str(args["query"]), max_results)
            return json.dumps(results, ensure_ascii=False, indent=2)

        if name == "read_context_doc":
            return _read_context_memory(str(args["path"]))

        if name == "list_context_docs":
            limit = _extract_int(args.get("limit")) or 20
            results = _list_context_memories(limit)
            return json.dumps(results, ensure_ascii=False, indent=2)

        if name == "read_file":
            path = _check_path(str(args["path"]))
            if not path.is_file():
                return f"Error: not a file: {path}"
            return path.read_text(encoding="utf-8")

        if name == "list_dir":
            path = _check_path(str(args.get("path", ".")))
            if not path.is_dir():
                return f"Error: not a directory: {path}"
            entries = []
            for item in sorted(path.iterdir(), key=lambda p: (not p.is_dir(), p.name.lower())):
                suffix = "/" if item.is_dir() else ""
                entries.append(f"{item.name}{suffix}")
            return "\n".join(entries)

        if name == "run_bash":
            command = str(args.get("command", ""))
            env = os.environ.copy()
            env["PATH"] = f"{CONDA_BIN}:{env.get('PATH', '')}"
            env["CONDA_PREFIX"] = str(CONDA_BIN.parent)
            env["CONDA_DEFAULT_ENV"] = CONDA_BIN.parent.name
            try:
                proc = await asyncio.create_subprocess_shell(
                    command,
                    stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.STDOUT,
                    cwd=PROJECT_ROOT,
                    env=env,
                )
                stdout, _ = await asyncio.wait_for(proc.communicate(), timeout=60.0)
                return stdout.decode(errors="replace")
            except asyncio.TimeoutError:
                proc.kill()
                return "Error: command timed out after 60s"

        if name == "web_search":
            query = str(args.get("query", ""))
            max_results = min(int(args.get("max_results", 5)), 10)

            def _do_search():
                return list(DDGS().text(query, max_results=max_results))

            results = await asyncio.to_thread(_do_search)
            result_json = json.dumps(results, ensure_ascii=False, indent=2)
            try:
                _write_skill("web_search", {"query": query}, result_json, ttl_days=7)
            except Exception:
                pass
            return result_json

        if name == "read_pdf":
            path = _check_path(str(args["path"]))
            source_mtime_val = path.stat().st_mtime
            if not path.is_file():
                return f"Error: not a file: {path}"
            max_chars = int(args.get("max_chars", 8000))

            def _do_pdf():
                doc = fitz.open(str(path))
                parts = []
                for i, page in enumerate(doc):
                    text = page.get_text()
                    if text.strip():
                        parts.append(f"--- Page {i + 1} ---\n{text}")
                doc.close()
                return "\n".join(parts)

            text = await asyncio.to_thread(_do_pdf)
            try:
                _write_skill(
                    "read_pdf",
                    {"path": str(path.relative_to(PROJECT_ROOT))},
                    text[:max_chars],
                    source_mtime=source_mtime_val,
                )
            except Exception:
                pass
            return text[:max_chars]

        if name == "find_skill":
            results = _list_skills(str(args.get("query", "")))
            return json.dumps(results, ensure_ascii=False, indent=2)

        return f"Error: unknown tool {name!r}"
    except Exception as e:
        return f"Error: {e}"


async def _agentic_call(
    messages: list[dict],
    profile: dict,
    slot_id: int,
    use_tools: bool = True,
    max_iter: int = 8,
    role: str = "general",
    prompt_scope: str = "",
) -> str:
    conversation = _with_system_prompts(messages, use_tools, role, prompt_scope)
    base_body = {
        "model": "local",
        "stream": False,
    }
    base_body.update({k: v for k, v in profile.items() if k != "extra_body"})
    base_body.update(profile.get("extra_body", {}))
    base_body["id_slot"] = slot_id
    if use_tools:
        base_body["tools"] = READ_TOOLS
        base_body["tool_choice"] = "auto"

    _ensure_context_cleanup_task()
    async with httpx.AsyncClient(timeout=None) as client:
        await _maybe_cleanup_slot(client, slot_id, _estimate_message_tokens(conversation))

        for _ in range(max_iter):
            body = dict(base_body)
            body["messages"] = conversation
            response = await client.post(f"{BASE_URL}/chat/completions", json=body)
            response.raise_for_status()
            data = response.json()
            _mark_slot_used(
                slot_id,
                data.get("usage") or data.get("timings"),
                _estimate_message_tokens(conversation),
            )
            message = data["choices"][0]["message"]
            tool_calls = message.get("tool_calls") or []

            if not use_tools or not tool_calls:
                content = message.get("content")
                if content is None:
                    await _finalize_context(client, slot_id)
                    return ""
                final_content = str(content)
                await _finalize_context(client, slot_id, final_content=final_content)
                return final_content

            conversation.append(message)
            for tool_call in tool_calls:
                function = tool_call.get("function", {})
                name = function.get("name", "")
                raw_args = function.get("arguments") or "{}"
                if isinstance(raw_args, str):
                    try:
                        args = json.loads(raw_args)
                    except json.JSONDecodeError as e:
                        result = f"Error: invalid JSON arguments: {e}"
                    else:
                        result = await _execute_tool(name, args)
                elif isinstance(raw_args, dict):
                    result = await _execute_tool(name, raw_args)
                else:
                    result = "Error: invalid tool arguments"

                conversation.append(
                    {
                        "role": "tool",
                        "tool_call_id": tool_call.get("id", ""),
                        "content": result,
                    }
                )

    final_content = f"Error: tool call iteration limit reached ({max_iter})"
    await _finalize_context(
        client,
        slot_id,
        "tool call iteration limit reached",
        final_content=final_content,
    )
    return final_content


async def _plan_agentic_call(
    messages: list[dict],
    profile: dict,
    slot_id: int,
    max_iter: int = 8,
    role: str = "teammate",
    prompt_scope: str = "plan",
) -> tuple[str, list[dict]]:
    conversation = _with_system_prompts(messages, True, role, prompt_scope)
    proposals: list[dict] = []
    base_body = {
        "model": "local",
        "stream": False,
    }
    base_body.update({k: v for k, v in profile.items() if k != "extra_body"})
    base_body.update(profile.get("extra_body", {}))
    base_body["id_slot"] = slot_id
    base_body["tools"] = PLAN_TOOLS
    base_body["tool_choice"] = "auto"

    _ensure_context_cleanup_task()
    async with httpx.AsyncClient(timeout=None) as client:
        await _maybe_cleanup_slot(client, slot_id, _estimate_message_tokens(conversation))

        for _ in range(max_iter):
            body = dict(base_body)
            body["messages"] = conversation
            response = await client.post(f"{BASE_URL}/chat/completions", json=body)
            response.raise_for_status()
            data = response.json()
            _mark_slot_used(
                slot_id,
                data.get("usage") or data.get("timings"),
                _estimate_message_tokens(conversation),
            )
            message = data["choices"][0]["message"]
            tool_calls = message.get("tool_calls") or []

            if not tool_calls:
                content = message.get("content")
                if content is None:
                    await _finalize_context(client, slot_id)
                    return "", proposals
                final_content = str(content)
                await _finalize_context(client, slot_id, final_content=final_content)
                return final_content, proposals

            conversation.append(message)
            for tool_call in tool_calls:
                function = tool_call.get("function", {})
                name = function.get("name", "")
                raw_args = function.get("arguments") or "{}"
                if isinstance(raw_args, str):
                    try:
                        args = json.loads(raw_args)
                    except json.JSONDecodeError as e:
                        result = f"Error: invalid JSON arguments: {e}"
                        conversation.append(
                            {
                                "role": "tool",
                                "tool_call_id": tool_call.get("id", ""),
                                "content": result,
                            }
                        )
                        continue
                elif isinstance(raw_args, dict):
                    args = raw_args
                else:
                    result = "Error: invalid tool arguments"
                    conversation.append(
                        {
                            "role": "tool",
                            "tool_call_id": tool_call.get("id", ""),
                            "content": result,
                        }
                    )
                    continue

                if name == "propose_write":
                    try:
                        path = _check_path(str(args["path"]))
                        proposal = {
                            "path": path.relative_to(PROJECT_ROOT).as_posix(),
                            "content": str(args["content"]),
                            "reason": str(args["reason"]),
                        }
                        proposals.append(proposal)
                        result = f"queued write proposal for {proposal['path']}"
                    except Exception as e:
                        result = f"Error: {e}"
                else:
                    result = await _execute_tool(name, args)

                conversation.append(
                    {
                        "role": "tool",
                        "tool_call_id": tool_call.get("id", ""),
                        "content": result,
                    }
                )

    final_content = f"Error: tool call iteration limit reached ({max_iter})"
    await _finalize_context(
        client,
        slot_id,
        "tool call iteration limit reached",
        final_content=final_content,
    )
    return final_content, proposals


@mcp.tool()
async def chat(
    messages: list[dict],
    mode: str = "coding",
    slot_id: int = EXCERPT_SLOT_ID,
    use_tools: bool = True,
    role: str = "general",
) -> str:
    if mode not in PROFILES:
        return f"Error: invalid mode {mode!r}"
    error = _validate_slot_id(slot_id)
    if error is not None:
        return error

    try:
        return await _agentic_call(
            messages,
            PROFILES[mode],
            slot_id,
            use_tools=use_tools,
            role=role,
            prompt_scope="explore" if role == "teammate" else "",
        )
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def chat_with_file(
    file_path: str,
    instruction: str,
    mode: str = "coding",
    slot_id: int = EXCERPT_SLOT_ID,
    role: str = "teammate",
) -> str:
    if mode not in PROFILES:
        return f"Error: invalid mode {mode!r}"
    error = _validate_slot_id(slot_id)
    if error is not None:
        return error

    try:
        path = _check_path(file_path)
        content = path.read_text(encoding="utf-8")
        messages = [
            {
                "role": "user",
                "content": (
                    f"Instruction:\n{instruction}\n\n"
                    f"File: {path.relative_to(PROJECT_ROOT).as_posix()}\n"
                    f"Content:\n{content}"
                ),
            },
        ]
        return await _agentic_call(
            messages,
            PROFILES[mode],
            slot_id,
            use_tools=True,
            role=role,
            prompt_scope="explore",
        )
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def chat_with_files(
    file_paths: list[str],
    instruction: str,
    mode: str = "coding",
    slot_id: int = EXCERPT_SLOT_ID,
    role: str = "teammate",
) -> str:
    if mode not in PROFILES:
        return f"Error: invalid mode {mode!r}"
    error = _validate_slot_id(slot_id)
    if error is not None:
        return error

    try:
        file_sections = []
        for file_path in file_paths:
            path = _check_path(file_path)
            content = path.read_text(encoding="utf-8")
            rel_path = path.relative_to(PROJECT_ROOT).as_posix()
            file_sections.append(f"File: {rel_path}\nContent:\n{content}")
        messages = [
            {
                "role": "user",
                "content": f"Instruction:\n{instruction}\n\n" + "\n\n".join(file_sections),
            },
        ]
        return await _agentic_call(
            messages,
            PROFILES[mode],
            slot_id,
            use_tools=True,
            role=role,
            prompt_scope="explore",
        )
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def generate_to_file(
    source_path: str,
    target_path: str,
    instruction: str,
    mode: str = "coding",
    slot_id: int = CODE_SLOT_ID,
    role: str = "implementation",
) -> str:
    if mode not in PROFILES:
        return f"Error: invalid mode {mode!r}"
    error = _validate_slot_id(slot_id)
    if error is not None:
        return error

    try:
        source = _check_path(source_path)
        target = _check_path(target_path)
        source_content = source.read_text(encoding="utf-8")
        messages = [
            {
                "role": "user",
                "content": (
                    f"Instruction:\n{instruction}\n\n"
                    f"Source file: {source.relative_to(PROJECT_ROOT).as_posix()}\n"
                    f"Target file: {target.relative_to(PROJECT_ROOT).as_posix()}\n\n"
                    f"Source content:\n{source_content}"
                ),
            },
        ]
        generated = await _agentic_call(
            messages,
            PROFILES[mode],
            slot_id,
            use_tools=True,
            role=role,
        )
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(generated, encoding="utf-8")
        return str(target)
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def health_check() -> str:
    health_url = f"{BASE_URL.removesuffix('/v1')}/health"
    try:
        async with httpx.AsyncClient(timeout=10.0) as client:
            response = await client.get(health_url)
        if response.status_code == 200:
            return "ok"
        return f"unavailable: {response.status_code}"
    except Exception as e:
        return f"unavailable: {e}"


@mcp.tool()
async def context_status(slot_id: int = -1) -> str:
    if slot_id != -1:
        error = _validate_slot_id(slot_id)
        if error is not None:
            return error

    try:
        selected_slots = SLOT_IDS if slot_id == -1 else (slot_id,)
        async with httpx.AsyncClient(timeout=None) as client:
            for selected_slot_id in selected_slots:
                await _refresh_slot_state(client, selected_slot_id, force=True)

        payload = _slot_status_payload(None if slot_id == -1 else slot_id)
        return json.dumps(payload, ensure_ascii=False, indent=2)
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def clear_context(slot_id: int = -1, reason: str = "manual clear") -> str:
    if slot_id != -1:
        error = _validate_slot_id(slot_id)
        if error is not None:
            return error

    try:
        selected_slots = SLOT_IDS if slot_id == -1 else (slot_id,)
        results = []
        async with httpx.AsyncClient(timeout=None) as client:
            for selected_slot_id in selected_slots:
                ok = await _erase_slot(client, selected_slot_id, reason, force=True)
                status = "cleared" if ok else "not cleared"
                state = _SLOT_CONTEXTS[selected_slot_id]
                detail = state.last_cleanup_error or state.last_cleanup_reason
                results.append(f"slot {selected_slot_id}: {status} ({detail})")
        return "\n".join(results)
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def remember_context(title: str, content: str, tags: list[str] | None = None) -> str:
    try:
        path = _write_context_memory(title, content, tags)
        return f"wrote {path}"
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def search_context(query: str, max_results: int = 8) -> str:
    try:
        results = _search_context_memories(query, max_results)
        return json.dumps(results, ensure_ascii=False, indent=2)
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def read_context_doc(path: str) -> str:
    try:
        return _read_context_memory(path)
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def list_context_docs(limit: int = 20) -> str:
    try:
        results = _list_context_memories(limit)
        return json.dumps(results, ensure_ascii=False, indent=2)
    except Exception as e:
        return f"Error: {e}"


# Manual-only helper; intentionally not exported as an MCP tool.
async def plan_edit(
    file_paths: list[str],
    instruction: str,
    mode: str = "coding",
    slot_id: int = CODE_SLOT_ID,
    role: str = "edit",
) -> str:
    if mode not in PROFILES:
        return f"Error: invalid mode {mode!r}"
    error = _validate_slot_id(slot_id)
    if error is not None:
        return error

    try:
        file_sections = []
        for file_path in file_paths:
            path = _check_path(file_path)
            content = path.read_text(encoding="utf-8")
            rel_path = path.relative_to(PROJECT_ROOT).as_posix()
            file_sections.append(f"File: {rel_path}\nContent:\n{content}")
        messages = [
            {
                "role": "system",
                "content": (
                    "You may read files, list directories, "
                    "and run bash commands for inspection. If edits are needed, use "
                    "propose_write with path, content, and reason. Do not use write_file."
                ),
            },
            {
                "role": "user",
                "content": f"Instruction:\n{instruction}\n\n" + "\n\n".join(file_sections),
            },
        ]
        analysis, proposals = await _plan_agentic_call(
            messages,
            PROFILES[mode],
            slot_id,
            role=role,
            prompt_scope="",
        )
        return json.dumps(
            {"analysis": analysis, "proposals": proposals},
            ensure_ascii=False,
            indent=2,
        )
    except Exception as e:
        return f"Error: {e}"


@mcp.tool()
async def plan_coordinate(
    file_paths: list[str],
    instruction: str,
    mode: str = "plan",
    slot_id: int = EXCERPT_SLOT_ID,
) -> str:
    if mode not in PROFILES:
        return f"Error: invalid mode {mode!r}"
    error = _validate_slot_id(slot_id)
    if error is not None:
        return error

    try:
        file_sections = []
        for file_path in file_paths:
            path = _check_path(file_path)
            content = path.read_text(encoding="utf-8")
            rel_path = path.relative_to(PROJECT_ROOT).as_posix()
            file_sections.append(f"File: {rel_path}\nContent:\n{content}")
        messages = [
            {
                "role": "system",
                "content": (
                    "You may read files, list directories, run targeted inspection "
                    "commands, and propose plan-document writes when the prompt "
                    "explicitly requests an update. For retrieval or audit-only "
                    "requests, do not propose writes."
                ),
            },
            {
                "role": "user",
                "content": f"Instruction:\n{instruction}\n\n" + "\n\n".join(file_sections),
            },
        ]
        analysis, proposals = await _plan_agentic_call(
            messages,
            PROFILES[mode],
            slot_id,
            role="teammate",
            prompt_scope="plan",
        )
        return json.dumps(
            {"analysis": analysis, "proposals": proposals},
            ensure_ascii=False,
            indent=2,
        )
    except Exception as e:
        return f"Error: {e}"


# Manual-only helper; intentionally not exported as an MCP tool.
async def apply_writes(proposals: list[dict]) -> str:
    try:
        if not proposals:
            return "no proposals to apply"

        results = []
        for proposal in proposals:
            path = _check_path(str(proposal["path"]))
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(str(proposal["content"]), encoding="utf-8")
            results.append(f"wrote {path.relative_to(PROJECT_ROOT).as_posix()}")
        return "\n".join(results)
    except Exception as e:
        return f"Error: {e}"


if __name__ == "__main__":
    mcp.run()
