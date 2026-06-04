---
description: Delegate local-LLM teammate tasks to save Claude's context window. llama:explore extracts source-backed excerpts; llama:plan handles routine plan retrieval/update/audit; llama:gen handles small implementation generation or pattern-following modifications.
disable-model-invocation: false
argument-hint: "[llama:explore \"<question>\" <file>...] | [llama:plan \"<retrieve/update/audit packet>\" <file>...] | [llama:gen <source> <target> \"<instruction>\"]"
allowed-tools: Read Write Edit Bash mcp__llama__health_check mcp__llama__chat_with_files mcp__llama__plan_coordinate mcp__llama__generate_to_file
---

## Input

`$ARGUMENTS` — parse the first token as the mode alias. Accepted aliases:
`llama:explore`/`--explore`, `llama:plan`/`--plan`, and `llama:gen`/`--gen`.

## Pre-flight

Call `mcp__llama__health_check`. If result ≠ `"ok"`, stop:
> llama-server is not running. Set `LLAMA_SERVER` and `LLAMA_MODEL` in `.env` and verify `start-llama.sh` can reach the model.

---

## Local LLM Roles

This skill uses two local-LLM roles across two llama-server slots:

| Role | Mode | Purpose |
|------|------|---------|
| `teammate` | `llama:explore`, `llama:plan` | Slot 0 teammate for source excerpts plus routine plan retrieval/update/audit |
| `implementation` | `llama:gen` | Slot 1 generator for boilerplate, small code generation, narrow tests/configs, or low-risk pattern-following modifications |

The local LLM has a training cutoff of approximately early 2025. It automatically calls `web_search` for post-training facts (changelogs, release notes, current events) and `context7_resolve_library` + `context7_query_docs` for library/API questions. It can also extract text from PDFs via `read_pdf`. These tools are always available to the LLM during any `llama:explore` or `llama:gen` call — delegate external-info lookup here rather than loading docs into Claude's context.

Role prompts are maintained in `.llama/prompt/*.md`; `.llama/llama_server.py`
loads those files by role and injects required workflow rules:

- `llama:plan` loads `.claude/rules/Edit_Workflow.md`.
- `llama:explore` loads only the slot-0 teammate prompt; it does not load `.claude/rules/Edit_Workflow.md`.
- `llama:gen` loads `.claude/rules/Guide_Behavior.md`.

The local LLM clears its slot after each work unit. It can persist exact facts in `.llama/llama-context/` via its context tools, but do not rely on local memory for canonical facts; re-query source files when precision matters.

Slot mapping assumes `llama-server --parallel 2`:

| Slot | Use |
|------|-----|
| `0` | Teammate: source exploration plus plan document retrieve/update/audit |
| `1` | Small implementation generation or pattern-following modification |

Use `llama:gen` for implementation work smaller than a Codex handoff:
boilerplate, pattern-following files, narrow generated tests/configs, small code
generation, or low-risk complete-file modifications. Use `codex-workflow` for
broader behavior changes, coupled source/test changes, or anything requiring
non-local design judgment.

---

## Mode: `llama:explore`

Use when Claude needs a specific fact, code path, snippet, dependency, symbol, or concise source-backed summary from large files without loading those files into Claude's context.

```
mcp__llama__chat_with_files(
  file_paths=[<files>],
  instruction="<question>",
  mode="plan",
  slot_id=0,
  role="teammate",
)
```

Return the response verbatim. Do not read the source files yourself.

---

## Mode: `llama:plan`

Use slot `0` for routine plan-coordinator work after plan initialization: targeted retrieval, explicit plan document updates with evidence, or narrow audits. Do not use it for initial `plan/` creation; use `init-plan`.

The first argument must be a single handoff packet in one mode:

```
Mode: retrieve | update | audit
Question/Change/Audit Target: ...
Sources/Destinations: ...
Evidence: ...
Expected Output: ...
```

Call:

```
mcp__llama__plan_coordinate(
  file_paths=[<plan/rule/source files named in the packet>],
  instruction="<handoff packet>",
  mode="plan",
  slot_id=0,
)
```

For retrieve/audit responses, return the `analysis` field verbatim and do not apply proposals. For update responses, display `analysis` and any `proposals`; apply proposals only after explicit user approval unless the user already gave direct approval for the exact plan change.

---

## Mode: `llama:gen`

Generate or revise one complete file for a small implementation work unit:
boilerplate, narrow tests/configs, small code generation, or low-risk
pattern-following modification.

**1.** If this is new-file generation and `<target>` already exists, warn:
"`<target>` already exists — overwrite?" Stop unless the user confirms.

For explicit small modifications, `<source>` and `<target>` may be the same
file. The instruction must name the exact requested change and stay within the
sub-Codex scope above.

**2.** Generate:
```
mcp__llama__generate_to_file(
  source_path=<source>,
  target_path=<target>,
  instruction="<instruction>",
  mode="coding",
  slot_id=1,
  role="implementation",
)
```

**3.** Verify syntax:
- Python → `python -c "import ast; ast.parse(open('<target>').read()); print('ok')"`
- Other → `head -5 <target>`

Report the written path and verification result.
