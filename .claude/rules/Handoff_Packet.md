---
description: Required handoff packet fields and per-mode query templates for all agent invocations.
---

# Agent Handoff Packet

**Team member response protocol:** An agent's plain text output is not delivered back to the team lead — only `SendMessage` calls are. When a response is required from any team member, include this line at the top of the message:
```
IMPORTANT: Reply using SendMessage addressed to "team-lead".
```

When invoking an agent, provide the fields relevant to that agent.

| Field | Applies To | Content |
|-------|------------|---------|
| User Request | all | Original user goal or narrowed task |
| Agent Role | all | Why this agent is being called now |
| Relevant Extracts | all | `plan-coordinator` output or exact source facts |
| Target Scope | all | Files, symbols, phase, or documents in scope |
| Out of Scope | all | What must not be changed, judged, or expanded |
| Success Criteria | all | Expected behavior, status update, review target, or answer shape |
| Expected Output | all | Report, patch result, status update, findings, or answer |
| Implementation Blueprint | `codex-writer-*` | Behavior contract, architecture constraints, and preferred verification commands |
| Verification Evidence | `code-reviewer`, `plan-coordinator` | Commands run, results, or evidence still missing |
| Review Contract | `code-reviewer` | User request, blueprint, writer report, plan extract, or verification result to review against |
| Review Scope | `code-reviewer` | File paths, diff range, or changed files to inspect |
| Mode | `plan-coordinator` | `retrieve`, `update`, or `audit` — one mode per call |
| Question or Change | `plan-coordinator` | Narrow retrieval question, explicit plan change, or audit target |
| Sources/Destinations | `plan-coordinator` | Source files, canonical destination files, or audit scope |
| Target Status | `plan-coordinator` status updates | Intended status term and the evidence supporting it |

## plan-coordinator Query Templates

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

Provide the exact evidence, the destination file and section, and the precise change. Do not leave plan-coordinator to infer what to write.

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