---
description: Use when defining or changing project scope, plan documents, or file structure.
paths:
  - "./plan/*.md"
---

## When To Use

- creating or modifying `plan/` documents
- changing canonical document structure or placement
- moving or deleting files

For trivial plan edits, apply only the relevant parts.

## Startup Routine

Before non-trivial work, gather context in this order:

1. `CLAUDE.md`
2. `plan/README.md`
3. request task-relevant planning facts through `plan-coordinator`
4. the files that will actually be changed

Main Model may read `plan/README.md` directly as the status and document-map entry point. Access other `plan/*.md` files, `plan.md`, and `structure.md` through `plan-coordinator`; initial plan creation uses the `init-plan` skill.

While gathering context, surface conflicts, stale state, missing context, or unclear goals.
Do not silently guess around inconsistencies.

## Canonical Document Rules

Do not keep the same information in multiple Markdown files.
Each type of information has one canonical document.
Non-canonical files should link or map to the canonical location instead of repeating details.

| Information | Canonical Location |
|-------------|--------------------|
| Project goal and scope | `plan/OVERVIEW.md` |
| Current status and document map | `plan/README.md` |
| Decision history | `plan/DECISIONS.md` |
| Work phases and procedure | `plan/PHASES.md` |
| Code structure and architecture | `plan/ARCHITECTURE.md` |
| Review, QA, fallback | `plan/REVIEW.md` |
| User-run tasks and local constraints | `plan/USER.md` |
| Executable scripts | `scripts/` |
| Reference code and templates | `snippets/` |

When the canonical structure changes, update directly affected references at the same time; this is part of the minimum required edit.
Create a new document only when it is necessary for the request.
Before creating a new canonical document, check whether an existing planning section is enough.

## Markdown Placement Rules

Choose the location before adding new Markdown content.

- status change -> `plan/README.md`
- goal or scope change -> `plan/OVERVIEW.md`
- decision change -> `plan/DECISIONS.md`
- procedure change -> `plan/PHASES.md`
- architecture change -> `plan/ARCHITECTURE.md`
- review or QA change -> `plan/REVIEW.md`
- local execution or user action -> `plan/USER.md`
- long example code -> `snippets/`
- executable code -> `scripts/`

Do not repeat long explanations or tables across documents.
Before deleting or moving content, confirm unique information has been preserved in the canonical location.
Do not do unrelated document cleanup, wording normalization, or formatting changes.

## Code And File Placement

- `scripts/`: operational scripts that are run directly
- `snippets/`: templates or reference code copied into real code later
- `src/` or `app/`: product/project source code
- `tests/`: tests
- `plan/`: plans, process, decisions, review notes
- `backup/`: original artifacts, previous versions, migration references

Do not put long code implementations inside Markdown.
Move long code to `scripts/`, `snippets/`, or the real source tree, and leave paths plus usage notes in Markdown.
Classify temporary root scripts into `scripts/` or `snippets/` early.
Only add new directories, helpers, wrappers, or frameworks when the existing structure cannot solve the request cleanly.

## Status Language

Use precise status terms:

- stub exists: file exists but implementation is not real yet
- draft written: content exists but has not been reviewed or executed
- generated: a tool or model produced the artifact
- verified: build, test, execution, or review has confirmed it

When status changes, ask `plan-coordinator` to update `plan/README.md`.
Status text should make the next action visible.
Do not mark work as `verified` without a specific passing build, test, execution, or review result. Record the next action instead.

## Moving Or Deleting Files

When moving or deleting files:

1. Search all references with `rg`.
2. Move or delete the file.
3. Update reference paths.
4. Run the relevant build, test, or document validation.
5. If it is a decision, ask `plan-coordinator` to record the reason in `plan/DECISIONS.md`.

Do not leave deleted paths in `CLAUDE.md`, `README.md`, or `plan/`.
If the deleted document has unique information, move that information to the canonical document first.
Do not move or delete files unrelated to the user's request.

## User Local Environment

Use `plan-coordinator` to keep `plan/USER.md` as the canonical place for commands the user must run, personal paths, API keys, hardware constraints, and local operating notes.

Minimize entries in `plan/USER.md`. Default to handling tasks autonomously.
Only escalate to the user when Claude genuinely cannot proceed without human action:
- secrets, credentials, or personal paths
- hardware connections or external services
- empirically determined constraints that require the user to run and observe
  (e.g., optimal model settings, timeout values, hardware-specific performance limits)

Do not guess or rewrite the user's server commands.
`plan/USER.md` is not a replacement for execution. It explains constraints and operating boundaries.

Separate roles clearly:

- user: server start/stop, personal paths, secrets, hardware connections
- Claude: input preparation, task splitting, result review, failure analysis
- automation scripts: repeated execution, minimal validation, logs, backups

Ask `plan-coordinator` to record concurrency, context size, timeout, GPU, or memory constraints in `plan/USER.md` or the relevant execution document.
Use those constraints when sizing tasks.

## Context Management

For non-trivial work, estimate scope and input/output size before starting.
If the task is likely to exceed the available context, ask `plan-coordinator` to record the current state and next action in `plan/README.md`, then pause.

Split long outputs by file or section.
Do not modify many canonical documents in one pass unless the request requires it.

## Verification

After work, run verification directly tied to the success criteria.
For non-trivial work, check at least:

- old paths or file names with `rg`
- Markdown links pointing at moved or deleted files
- whether `plan-coordinator` needs to update `plan/README.md` status
- whether `plan-coordinator` needs to add a `plan/DECISIONS.md` decision note

If verification cannot run, say why and list the remaining check.
