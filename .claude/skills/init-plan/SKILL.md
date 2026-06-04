---
description: Initialize plan/ documents for a new project. Opus reads the bootstrap specs and writes the canonical plan/ documents directly, with no subagents.
disable-model-invocation: true
model: opus
effort: high
tools: Read, Glob, Grep, Write, Edit
---

Initialize the `plan/` directory for this project, working entirely as Opus with
no subagents. The bootstrap specs are small enough to read directly, and Opus
must compose the full document content anyway, so delegating reading or writing
only duplicates context. Read directly, decide directly, write directly,
verify directly.

## Step 1 — Bootstrap Context

Read these directly:
- The user's project description provided when invoking this skill
- Root bootstrap documents such as `plan.md`, `structure.md`, architecture
  notes, migration notes, or other user-provided initial specs
- Canonical document table from `.claude/rules/Edit_Workflow.md`

If any `plan/` documents already exist, list them and ask the user whether to
proceed before continuing.
If no project description was provided, ask the user for one before proceeding.

## Step 2 — Extract Facts

From the bootstrap context, pull the facts each canonical plan document needs:
- Project goal, scope, and constraints
- Defined phases or procedures
- User-run tasks, secrets, hardware, or empirically determined values
- Any prior decisions or architecture notes
- Current status of any existing work

Preserve exact names, paths, commands, constants, and status terms. Record
explicit contradictions or TODOs in the source as open questions; do not invent
a resolution. If a needed fact is absent, mark it as a missing fact / next
action — do not fabricate a substitute. For large or ambiguous sources, reread
the specific section directly rather than guessing.

## Step 3 — Planning Gate

Decide:
- Which canonical `plan/` documents to create or update (see
  `.claude/rules/Edit_Workflow.md` for placement)
- Which extracted facts belong in each document, kept in one canonical location
- Which facts are missing, contradictory, or too weak to record as requirements
- Which unresolved items become next actions or open questions

Create only the canonical documents supported by the facts. Do not invent
requirements, architecture, phases, decisions, verification evidence, or status.

## Step 4 — Write Documents

Write the planned `plan/` documents directly with Write/Edit.

- Keep each fact in its canonical document; link (`[[name]]`) instead of
  duplicating long specs, tables, or architecture detail across documents.
- Before editing an existing document, read it first and preserve unrelated
  headings, links, tables, status shapes, and wording.
- Mark newly created plan documents as `generated` in `plan/README.md` unless
  reviewed or verified evidence exists. Use only the allowed status terms from
  `.claude/rules/Edit_Workflow.md`; `verified` requires explicit evidence.

## Step 5 — Self-Audit

After writing, audit the created or edited files directly:
1. Each fact is in the correct canonical document.
2. Markdown links point to existing files or intentionally-future documents.
3. Status entries use only the allowed status terms.
4. `verified` appears only with explicit verification evidence.
5. Missing facts are represented as next actions or open questions, not invented
   content.
6. The plan does not contradict the source evidence.

Also run the `Edit_Workflow.md` verification checks: `rg` for stale paths, and
confirm `plan/README.md`'s document map matches the files actually created. Fix
issues directly when the correct fix is evidence-supported; otherwise report
them.

## Step 6 — Report Output

Report which documents were created, which were skipped, the self-audit result,
and any contradictions or follow-up questions.