## Language

- Document-editing language: English
- Communication language with user via terminal: Korean

## Required Reading On New Sessions

1. `plan/README.md` — current status and canonical document map

If `plan/README.md` does not exist, invoke the `init-plan` skill before large implementation work.

## AI Orchestration Protocol

Never read source or plan files directly; never edit source files directly — classify scope from the table below, then read and invoke the matching skill (`call-llama` for llama modes, `codex-workflow` for codex and review).

`llama:explore/plan → Main Model → [llama:gen | codex:rescue] → [code-reviewer + codex:review] → Main Model → llama:plan`

| Component | Type | Use |
|-----------|------|-----|
| `llama:explore` | MCP | Source facts, external docs (changelogs, PDFs) — without loading files here |
| `llama:plan` | MCP | Plan documents: retrieve, update, or audit `plan/*.md` |
| `llama:gen` | MCP | Single-file: boilerplate, small codegen, narrow tests/configs, low-risk modifications |
| `codex` | plugin | Cross-file reasoning, non-trivial logic, coupled source/test changes |
| `code-reviewer` | subagent | Final review gate; spawn per task, in parallel with `codex:review` |

## Prompt Cache Policy

Keep this file as a lightweight router. Do not duplicate canonical plan facts, root specs, architecture tables, or phase details here — canonical document placement is defined in `.claude/rules/Edit_Workflow.md`.

For tasks requiring user intervention (server start/stop, secrets, local paths, hardware), use `call-llama` to retrieve user-local constraints from plan documents first.

## Rules

- `.claude/rules/Guide_Behavior.md` — think before coding, simplicity, surgical changes, goal-driven execution
- `.claude/rules/Edit_Workflow.md` — startup routine, canonical documents, file placement, status language, verification steps