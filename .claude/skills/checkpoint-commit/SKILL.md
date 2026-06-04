---
description: Create a focused git checkpoint after a completed, verified work unit. Use whenever a finished implementation, documentation, planning, or configuration step should be committed before starting the next step.
model: claude
tools: Bash, Read
---

## Purpose

Commit one completed, verified work unit without absorbing unrelated changes.

## Steps

### 1. Define the work unit

State the completed work in one sentence. Do not commit partially implemented or unverified work.

### 2. Inspect changes

```bash
git status --short
git diff -- <relevant paths>
```

Identify which files belong to this unit. Treat pre-existing or unrelated changes as user-owned — do not stage them.

### 3. Verify

Run the smallest meaningful check for the work unit. Use commands from `AGENTS.md`, `CLAUDE.md`, or the canonical plan. If verification cannot run, explain why before proceeding.

### 4. Stage only relevant files

```bash
git add <path> [<path> ...]
```

Never use `git add .` or `git add -A` when unrelated changes may exist.

### 5. Review staged patch

```bash
git diff --cached --stat
git diff --cached
```

### 6. Commit

Use an imperative summary. Add a short body with 2–5 `-` bullets covering material changes and verification result. End with a `Co-authored-by` trailer.

```bash
git commit \
  -m "<imperative summary>" \
  -m $'- <material change>\n- <material change>\n- Verify: <command and result>' \
  -m "Co-authored-by: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

For phase or milestone work, prefix the summary:

```
Phase N: <imperative summary>
```

### 7. Report

Run `git status --short` and `git log -1 --oneline`. Report the commit hash, subject, verification evidence, and any remaining uncommitted files.

## Guardrails

- Do not amend, rebase, reset, or rewrite existing commits unless the user explicitly requests it.
- Do not commit generated caches, secrets, `.env`, `settings.local.json`, or files excluded by `.gitignore`.
- Stop and explain when intended changes cannot be separated cleanly from unrelated work.