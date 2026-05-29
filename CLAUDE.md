## Language

- Document-editing language: English
- Communication language with user via terminal: Korean

## AI Orchestration Protocol

For routine tasks, use the agent pipelines below to optimize token efficiency and cognitive clarity. The Main Model owns routing, planning judgment, conflict resolution, and design decisions; helper agents execute bounded roles. Prioritize successful execution over strict isolation.

**Workflow Map**

- **Implementation**: `plan-coordinator -> Main Model -> codex-writer-small|codex-writer-large -> code-reviewer -> Main Model -> plan-coordinator`
- **Plan coordination**: `Main Model <-> plan-coordinator`
  - Use this path for plan fact retrieval, plan document updates, and targeted consistency audits after initialization.

**Agent Lifecycle**

- `plan-coordinator`: Persistent team member. Spawn once per session via `TeamCreate` + `Agent(team_name=..., name="plan-coordinator")`; communicate throughout the session via `SendMessage(to: "plan-coordinator", ...)`. Goes idle between turns but does not terminate.
- `codex-writer-small`, `codex-writer-large`, `code-reviewer`: Regular subagents. Spawn per task via `Agent`; terminate automatically after returning results.

**Task Allocation And Prompt Cache Routing**

Before non-trivial Codex writer delegation, classify the work and choose the smallest useful number of agents.

| Classification | Routing |
|----------------|---------|
| Direct edit | 0 agents when direct editing is cheaper than coordination |
| Low-risk bounded task | 1 `codex-writer-small` when behavior, target files/symbols, constraints, and verification are explicit |
| High-risk coupled task | 1 `codex-writer-large` when bounded multi-file work needs deeper reasoning to keep architecture, ownership, lifetime, threading, or public API coherent |
| Independent split | 2-3 writers for independent short scopes without overlapping files |
| Type-gated parallel | Main Model writes shared type headers directly first, then spawns multiple parallel writers for packages that depend only on those types (not each other's implementation) |
| Sequential split | Sequential writers when each result defines the next safe edit boundary |
| Ambiguous | Gather more facts before invoking writers |

| Prompt Cache Rule | Action |
|-------------------|--------|
| Shared context | Keep source facts, constraints, out-of-scope rules, and success criteria stable and early in each handoff packet |
| Task-specific context | Put each writer's target scope and delta later |
| Split threshold | Prefer one cached writer call when splitting would duplicate large context or create coordination risk |
| Agent cap | Avoid more than 3 implementation writers unless each package has independent files, independent verification, and stable shared context |

**Agent Roles**

| Agent | Role |
|-------|------|
| Main Model | Owns architecture, decomposition, prompt-cache-aware routing, ambiguity resolution, final verification judgment, and follow-up decisions |
| `plan-coordinator` | Persistent team member (TeamCreate). Retrieves plan facts, records explicit progress and requirement changes, updates canonical plan documents from provided evidence, and runs targeted audits after initialization |
| `codex-writer-small` | Executes low-risk bounded implementation when behavior, target files/symbols, constraints, and verification are explicit |
| `codex-writer-large` | Executes high-risk coupled implementation that needs deeper reasoning for architecture, ownership, lifetime, threading, public API, or cross-cutting behavior |
| `code-reviewer` | Read-only integration reviewer for defects, scope drift, verification gaps, and plan-update needs |

Claude may write code directly only when direct editing is expected to use fewer tokens or less coordination than invoking a Codex writer, or after sub-agents fail to resolve a bug. When intervening directly on a larger task, provide the architectural blueprint or core reference implementation, then let Codex writers handle the remaining mechanical file writing where practical.

## Agent Handoff Packet

When invoking an agent, follow `.claude/rules/Handoff_Packet.md` for required fields and per-mode query templates.

## Prompt Cache Policy

Keep this file as a lightweight router. Do not duplicate canonical plan facts, root specs, architecture tables, phase details, algorithm catalogs, or long constraints here.

Main Model may read `plan/README.md` directly as the status and document-map entry point. Do not directly read any other `plan/*.md` file, `plan.md`, or `structure.md`; request the needed facts through `plan-coordinator`. If `plan/README.md` does not exist, use the `init-plan` skill.

Canonical document placement is defined in `.claude/rules/Edit_Workflow.md`.

## Required Reading On New Sessions

1. `CLAUDE.md` (this file)
2. `plan/README.md` - current status and canonical document map

If `plan/README.md` does not exist, the plan has not been created yet. Invoke the `init-plan` skill before large implementation work.

## Rules

- `.claude/rules/Guide_Behavior.md` - think before coding, simplicity, surgical changes, goal-driven execution
- `.claude/rules/Edit_Workflow.md` - startup routine, canonical documents, file placement, status language (`stub exists` / `draft written` / `generated` / `verified`), verification steps

For tasks that may require user intervention, ask `plan-coordinator` for user-local constraints before requesting server start/stop, secrets, local paths, hardware access, external service access, or manual observation from the user.

To create or modify the project workflow architecture, follow `Edit_Workflow`.
