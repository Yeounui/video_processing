---
description: Initialize plan/ documents for a new project by orchestrating bootstrap-only init-plan-reader and init-plan-writer agents.
disable-model-invocation: true
model: opus
effort: high
tools: Read, Glob, Agent
---

Initialize the plan/ directory for this project using the following steps.

Flow: `init-plan(opus) -> init-plan-reader -> init-plan(opus) -> init-plan-writer -> init-plan(opus)`.

## Step 1 — Bootstrap Context

Read these directly (they are small enough to pass without retrieval overhead):
- The user's project description provided when invoking this skill
- Root bootstrap documents such as `plan.md`, `structure.md`, architecture notes, migration notes, or other user-provided initial specs
- Canonical document table from `.claude/rules/Edit_Workflow.md`

If any `plan/` documents already exist, list them and ask the user whether to proceed before continuing.
If no project description was provided, ask the user for one before proceeding.

## Step 2 — Compose Query List

Based on the direct bootstrap context and what each canonical plan document requires, prepare a query list covering:
- Project goal, scope, and constraints
- Defined phases or procedures
- User-run tasks, secrets, hardware, or empirically determined values
- Any prior decisions or architecture notes
- Current status of any existing work

## Step 3 — Invoke init-plan-reader

Use `init-plan-reader` for targeted rereads, supplemental extraction, or mechanical extraction after the `init-plan` opus controller's direct first pass. The reader may read files Opus already read, including `plan.md`, `structure.md`, and other root bootstrap specs, when Opus needs exact facts, section-level extraction, conflict listing, or comparison with other sources. Do not delegate interpretation of those documents to the reader; pass narrow reread questions or exact extraction targets.

Spawn the `init-plan-reader` agent with:

- **Assigned Files**: paths to read directly (targeted rereads, exact wording, conflict detection)
- **Passed Excerpts**: exact sections from Step 1 reads needed for context without full rereads
- **Query List**: one numbered query per fact; reader responds per number
  ```
  Query 1: [exact extraction question — name file and section if known]
  Query 2: ...
  ```

Wait for all answers. On `NOT_FOUND`: record as missing fact or open question in Step 4; do not ask reader to infer a substitute.

## Step 4 — Opus Planning Gate

As the `init-plan` skill controller, use the direct bootstrap context plus the `init-plan-reader` answers to decide:
- Which canonical `plan/` documents should be created or updated
- Which extracted facts belong in each document
- Which facts are missing, contradictory, or too weak to record as requirements
- Which unresolved items should become next actions or open questions
- Which evidence and constraints must be passed verbatim to `init-plan-writer`

Do not ask `init-plan-writer` to resolve contradictions, infer missing scope, invent architecture, or decide document coverage.

## Step 5 — Invoke init-plan-writer

The `init-plan` opus controller must provide the interpretation and document plan. Treat `init-plan-writer` as a mechanical writer: it should not decide document coverage, canonical placement, conflict resolution, or whether weak facts become requirements.

Spawn the `init-plan-writer` agent using this handoff template:

| Field | Content |
|-------|---------|
| **Mode** | `bootstrap-write` |
| **Controller Decision** | Step 4 coverage, placement, conflict handling, skipped facts, and open-question decisions |
| **Direct Bootstrap Context** | Exact facts or excerpts from Step 1 root specs selected for writing |
| **Reader Facts** | Complete Step 3 supplemental answers or exact excerpts selected for writing |
| **Documents To Create/Update** | Exact `plan/...` file paths |
| **Document Bodies** | For each document: heading names with exact text under each — opus supplies full content, no gaps for init-plan-writer to infer. |
| **Preserve Rules** | Existing headings, links, tables, status shapes, or wording that must remain unchanged |
| **Do Not Write** | Facts, documents, sections, or interpretations excluded by Step 4 |
| **Self-Audit Scope** | Standard — init-plan-writer runs its built-in 6-point self-audit. Add specific exclusions or extra checks only if needed. |
| **Expected Output** | Changed files, skipped files, self-audit result, and missing instructions |

Instruct `init-plan-writer` to read `.claude/rules/Edit_Workflow.md` for canonical document placement and status terms. Instruct it to create only the canonical plan documents supported by the supplied facts, avoid inferred requirements, invented status, or speculative architecture, and run its built-in self-audit after writing.

Wait for `init-plan-writer` to finish writing and self-auditing the created documents.

## Step 6 — Opus Sanity Check

Read the `init-plan-writer` report and, when needed, inspect the created `plan/` files directly.
Do not spawn a separate reviewer by default.
Only request additional review if the project description or source documents conflict, existing `plan/` documents were merged, canonical placement is unclear, or the user explicitly asks for review.

## Step 7 — Report Output

Report which documents were created, which were skipped, the self-audit result, and any issues or follow-up questions from `init-plan-writer`.
