---
description: Set up or refresh project-scoped Codex configuration from the current Claude Code workflow. Invoke when initializing dual-tool support, after changing Claude agents, skills, rules, or hooks, or before switching to Codex as the active tool.
---

# Codex Project Setup

Create or refresh the Codex project files from the current Claude Code
configuration. Preserve unrelated existing files and keep generated Codex files
in Codex locations.

## Output Boundaries

| Content | Codex Destination |
|---------|-------------------|
| Root project instructions | `AGENTS.md` |
| Scoped behavioral instructions | `<scope>/AGENTS.md` |
| Project skills | `.agents/skills/<skill>/` |
| Project configuration | `.codex/config.toml` |
| Agent role prompts | `.codex/agents/*.toml` |
| Hooks and portable hook scripts | `.codex/hooks.json`, `.codex/hooks/` |
| Shared command approval policy | `.codex/rules/*.rules` |

Do not write Codex skills under `.claude/skills/`. Do not put behavioral
instructions in `.codex/rules/`; reserve that directory for command approval
policy. Keep all generated Codex configuration project-scoped. Do not read or
write global Codex configuration. If a Codex destination is not writable, stop
and ask the user to make that destination writable. Do not write the file to an
alternate location.

## Step 1 - Inspect Sources And Existing Output

Read:

1. `CLAUDE.md`
2. `plan/README.md`
3. `plan/USER.md`
4. `.claude/agents/`
5. `.claude/skills/`
6. `.claude/rules/`
7. `.claude/settings.json`
8. Existing `AGENTS.md`, nested `AGENTS.md`, `.agents/`, and `.codex/`

Build a source-to-destination inventory for Claude agents, skills, rules, and
every top-level `.claude/settings.json` key. For each source item, record one
result: migrate, already represented, or skip with reason. Report the files that
need creation or refresh before editing.

## Step 2 - Create Or Refresh Root `AGENTS.md`

Translate the project-wide workflow from `CLAUDE.md` into root `AGENTS.md`.
Keep it concise and operational.

Include:

- Required startup reading
- Workflow map and agent lifecycle
- Task allocation and agent roles
- Common work packet fields
- `llama:plan` query templates
- Plan access boundaries
- Project-wide behavioral rules
- Project commands

Adapt Claude-specific names and tools:

| Claude Code | Codex |
|-------------|-------|
| `TeamCreate`, `Agent`, `SendMessage` | `spawn_agent`, `send_input`, `wait_agent`, `resume_agent`, `close_agent` |
| `codex-writer-small`, `codex-writer-large` | `code-writer-small`, `code-writer-large` |

For `.claude/rules/Guide_Behavior.md`, merge the concise project-wide rules into
root `AGENTS.md`. Do not copy examples or explanatory sections unless they are
required for execution.

For `.claude/rules/Handoff_Packet.md`, merge common fields and query templates
into root `AGENTS.md`. Put role-specific packet requirements in the matching
`.codex/agents/*.toml` files.

Preserve operational routing details when they apply: type-gated parallel work,
integration-boundary review, and checking user-local constraints before asking
for manual intervention. Omit Claude prompt-cache guidance.

## Step 3 - Convert Scoped Behavioral Rules

For each `.claude/rules/*.md` file with `paths` frontmatter:

1. Parse each `paths` pattern.
2. Find the narrowest directory prefix before any wildcard.
3. Create or update `<directory>/AGENTS.md`.
4. Merge only the instructions required in that scope.
5. Remove Claude frontmatter from the Codex output.
6. Replace Claude-specific names, paths, and tools with Codex equivalents.
7. Avoid duplicating instructions already present in a parent `AGENTS.md`.
8. If the source glob is narrower than the target directory, add one
   applicability sentence that preserves the narrower scope.

Example:

```text
./plan/*.md -> plan/AGENTS.md
```

```markdown
Apply these instructions when modifying Markdown planning documents under
`plan/`.
```

When several patterns share a directory but need different instructions, keep
the applicability conditions explicit and short.

## Step 4 - Create Or Refresh Agent Roles

Ensure `.codex/agents/` contains standalone TOML prompts for Codex-backed roles:

- `code-writer-small`
- `code-writer-large`
- `code-reviewer`

Routine plan coordination is handled by the local LLM through `llama:plan`
in this Claude template, not by a Codex agent. Keep each prompt bounded to that role. Use `name`, `description`,
`developer_instructions`, and role-specific runtime settings. Do not use
`role`, per-agent `config_file` registration, or Claude team response
instructions. Agents return final reports to the parent.

Use this project configuration shape:

```toml
model = "gpt-5.5"
model_reasoning_effort = "high"
sandbox_mode = "workspace-write"
approval_policy = "on-request"

[features]
hooks = true

[agents]
max_threads = 4
max_depth = 1
```

Use these defaults unless the installed Codex model catalog requires a
supported substitute:

| Role | Model | Reasoning | Sandbox |
|------|-------|-----------|---------|
| Main Model | `gpt-5.5` | `high` | `workspace-write` |
| `code-writer-small` | `gpt-5.4` | `medium` | `workspace-write` |
| `code-writer-large` | `gpt-5.5` | `high` | `workspace-write` |
| `code-reviewer` | `gpt-5.4` | `high` | `read-only` |

Preserve useful existing descriptions and nickname candidates. Keep
`max_depth = 1` unless an agent must delegate to another agent.

## Step 5 - Create Or Refresh Project Skills

Review every `.claude/skills/*/SKILL.md`. Create Codex project skills only under
`.agents/skills/<skill>/`.

For each migrated skill:

1. Keep `SKILL.md` focused on reusable execution instructions.
2. Use Codex skill frontmatter with `name` and `description`.
3. Add `agents/openai.yaml` when UI metadata is useful.
4. Validate with `quick_validate.py`.
5. Record the Claude source and Codex destination in the inventory.

Do not silently skip a Claude skill. If it has no Codex migration, report the
reason. Do not copy Claude-only metadata fields into Codex skill frontmatter.

For this workflow, keep `init-plan` Claude-only by design. Record it as skipped:
Claude bootstraps the initial canonical plan before Codex project setup.

## Step 6 - Create Or Refresh Hooks

Review every key in `.claude/settings.json`, including `hooks`,
`enabledPlugins`, and `env`. Update `.codex/hooks.json` and place portable hook
scripts under `.codex/hooks/`.

For each Claude hook:

1. Preserve the event intent using a supported Codex hook event.
2. Keep commands portable and project-relative.
3. Ensure `Stop` hooks return valid JSON on successful exit.
4. Record the Claude source and Codex destination in the inventory.

For Conda setup:

1. Store the selected environment name in local-only `.conda-env`.
2. Ensure `.gitignore` excludes `.conda-env`. Do not commit the selected name.
3. Use `.codex/hooks/run-in-conda.sh` for project commands.
4. Add `.codex/hooks/session-start.sh` to provide the selected environment as
   `SessionStart` context.
5. Route test hooks through `run-in-conda.sh`.

Keep personal paths out of committed hook files.
Do not add a full-suite `Stop` hook automatically. Preserve an existing
project `Stop` hook or add one only when the user explicitly requests it.
Report that changed project hooks require review with `/hooks` in Codex.

## Step 7 - Add Command Policy Only When Needed

When the repository intentionally shares project command approval rules, create
`.codex/rules/default.rules`:

```python
prefix_rule(
    pattern = ["<command>", "<arg>"],
    decision = "allow",
    justification = "<why this command prefix is safe>",
)
```

Do not create global allowlists. Validate shared rules with
`codex execpolicy check`.

## Step 8 - Verify And Report

Verify:

1. Every created or modified Codex file is inside the repository, and the
   project is trusted so Codex loads the project `.codex/` layer.
2. `AGENTS.md` and required nested `AGENTS.md` files exist.
3. `.codex/config.toml` uses `[agents] max_depth = 1` unless recursion is needed.
4. Standalone agent TOML files parse, use supported fields, and reference Codex
   paths.
5. Codex project skills exist only under `.agents/skills/`.
6. `.codex/config.toml` enables hooks with `[features].hooks = true`.
7. Hook commands reference existing portable scripts.
8. `Stop` hook scripts return valid JSON when they exit successfully.
9. Shared `.codex/rules/*.rules` files contain command policy only.
10. `rg` finds no stale Codex references to moved Claude paths or global Codex
    output paths.
11. `codex debug models` contains the configured models.
12. `codex doctor --json` reports no `config.load` startup warnings.
13. Every Claude agent, skill, rule, and `.claude/settings.json` top-level key
    has an inventory result.

Report created and updated files, validations run, skipped optional sections,
and inventory skips with reasons. Report `init-plan` as intentionally
Claude-only. Do not create or request global Codex configuration.
