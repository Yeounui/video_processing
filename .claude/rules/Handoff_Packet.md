---
description: Required handoff packet fields and per-mode query templates for all agent invocations.
---

# Agent Handoff Packet

**Claude agent response protocol** (`Agent()` spawns only — does not apply to `Skill()` invocations): An agent's plain text output is not delivered back to the team lead — only `SendMessage` calls are. When a response is required from any team member, include this line at the top of the message:
```
IMPORTANT: Reply using SendMessage addressed to "team-lead".
```

When invoking an agent, provide the fields relevant to that agent.

| Field | Applies To | Content |
|-------|------------|---------|
| User Request | all | Original user goal or narrowed task |
| Agent Role | all | Why this agent is being called now |
| Relevant Extracts | all | `llama:explore`/`llama:plan` output or exact source facts |
| Target Scope | all | Files, symbols, phase, or documents in scope |
| Out of Scope | all | What must not be changed, judged, or expanded |
| Success Criteria | all | Expected behavior, status update, review target, or answer shape |
| Expected Output | all | Report, patch result, status update, findings, or answer |
| Implementation Blueprint | `codex:rescue` | Behavior contract, architecture constraints, and preferred verification commands |
| Verification Evidence | `code-reviewer`, `llama:plan` | Commands run, results, or evidence still missing |
| Review Contract | `code-reviewer` | User request, blueprint, writer report, plan extract, or verification result to review against |
| Review Scope | `code-reviewer` | File paths, diff range, or changed files to inspect |
| Mode | `llama:plan` | `retrieve`, `update`, or `audit` — one mode per call |
| Question or Change | `llama:plan` | Narrow retrieval question, explicit plan change, or audit target |
| Sources/Destinations | `llama:plan` | Source files, canonical destination files, or audit scope |
| Target Status | `llama:plan` status updates | Intended status term and the evidence supporting it |
| Exploration Question | `llama:explore` | Specific source fact, symbol, behavior, dependency, or path to extract |
| Source Files | `llama:explore` | Exact files or directories to inspect |

For `codex:rescue` (`Skill()` invocation), no SendMessage protocol applies. Compose the `args` string using:

```
Task: [User Request]
Scope: [target files/symbols]
Out of scope: [do not touch]
Context: [plan facts / architecture constraints]
Blueprint: [behavior contract, API constraints, verification commands]
Done when: [success criteria]
```

## llama:gen Instruction Template

Compose a tight `<instruction>` string covering:

```
Generate [new file | modification to <target>] following the pattern in <source>.
Preserve: [naming / imports / style / structure to keep from source]
Fill: [exact surface — function names, test cases, config keys]
Do not change: [symbols, sections, or behavior to leave untouched]
Done when: [what correct output looks like]
```

Keep under 6 lines. The local LLM generates one complete file; do not give multi-file tasks or design decisions here.

## llama:explore Query Template

Use this for code or document exploration that should not load
`.claude/rules/Edit_Workflow.md`.

```
Question: [single specific source question]
Sources: [file paths or directories]
Need: [symbols, call path, constants, short excerpts, or NOT_FOUND]
Out of Scope: [implementation, edits, broad summary, unrelated files]
Expected Output: [dense source-backed bullets with paths and line/symbol references]
```

## llama:plan Query Templates

**One mode per call.** Do not combine `retrieve` + `update` or `update` + `audit` in a single message.

### Retrieve

State one question. Name the exact source file and section. Say what form the answer should take.

```
Mode: retrieve
Question: [single specific question]
Sources: [file path] § [section heading]
Expected Output: [answer shape — e.g., "exact status term", "bulleted constraints", "all verification commands"]
```

Example:
```
Mode: retrieve
Question: What are the acceptance criteria for Phase 3?
Sources: plan/PHASES.md § Phase 3
Expected Output: Bulleted list, exact wording from source.
```

### Update

Provide the exact evidence, the destination file and section, and the precise change. Do not leave the local LLM to infer what to write.

```
Mode: update
Change: [exact edit — what to add, replace, or remove, and where in the section]
Destination: [file path] § [section heading]
Evidence: [quote or concrete result that justifies the change]
Target Status: [new status term if this is a status update]
Expected Output: Confirm which file/section changed; report the new text verbatim.
```

Example:
```
Mode: update
Change: Replace Phase 2 status with "verified"
Destination: plan/README.md § Current Status
Evidence: cmake --build build succeeded (exit 0); window rendered on screen (user confirmed).
Target Status: verified
Expected Output: Confirm the status line updated; report new text.
```

### Audit

Name a specific document or section pair. Do not request a full plan audit in one call.

```
Mode: audit
Audit Target: [document or section pair — e.g., "plan/DECISIONS.md § D40 vs plan/ARCHITECTURE.md § Startup Sequence"]
Expected Output: Findings list, or explicit "consistent" confirmation with checked items.
```
