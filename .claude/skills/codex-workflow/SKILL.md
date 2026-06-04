---
description: Task classification and Codex invocation. Read before any implementation work on source or test files.
allowed-tools: Bash
---

# Codex Workflow

## Invocation

```
Skill("codex:rescue", args="<flags> <task>")   # implementation
Skill("codex:review")                           # review, medium effort
Skill("codex:review", args="--effort high")     # multi-file or final gate
```

**PATH 문제로 codex 호출이 실패할 경우** (conda activate codex 없이 claude를 실행했을 때), 다음 명령어로 직접 호출합니다:

```bash
export PATH="$(dirname "$CONTEXT7_NODE"):$PATH" && \
"$CONTEXT7_NODE" "$CODEX_COMPANION" task <flags> "<task>"
```

Rescue flags (do not call rescue from inside a rescue subagent):

| Flag | Values | Notes |
|------|--------|-------|
| `--model` | `gpt-5.4` · `gpt-5.5` | low-risk · high-risk |
| `--effort` | `medium` · `high` | gpt-5.4 → medium; gpt-5.5 → high |
| `--write` | — | always add for implementation |
| `--resume` | — | continue last task thread; skips prompt; review jobs don't overwrite |
| `--fresh` | — | force new thread |
| `--background` | — | non-blocking; returns `jobId`; pair with `--fresh` for fan-out |

Run `codex:review` in parallel with `code-reviewer` as a final review gate per task.

## Task Classification

| Classification | Flags / Routing |
|----------------|-----------------|
| Trivial mechanical edit | Direct edit: single file, ≤3 lines, no logic change. Bodies/deletions/test logic = not trivial. **Doubt → Low-risk.** |
| Low-risk bounded task | `--model gpt-5.4 --effort medium --write` |
| High-risk coupled task | `--model gpt-5.5 --effort high --write` |
| Independent split | Tasks share no files or dependencies — sequential calls, flags per classification |
| Type-gated | Main Model writes shared types/interfaces first, then sequential calls for dependents |
| Sequential split | Each rescue output defines the next task's scope — sequential calls |
| Ambiguous | Gather more facts first |

## Patterns

**Sequential:** rescue → parallel review (code-reviewer + codex:review) → if issues: `codex:rescue --resume "<issue description>"` → repeat.

**Fan-out:** `--fresh --background --write <task>` per task → `/codex:status <jobId>` → `/codex:result <jobId>` → review each result independently. Specific-thread follow-up: `codex resume <threadId>` (shown in `/codex:result`); confirm with user.

## Handoff Packet

Follow `.claude/rules/Handoff_Packet.md`. Key fields: User Request, Target Scope, Out of Scope, Implementation Blueprint, Success Criteria, Verification Evidence.
