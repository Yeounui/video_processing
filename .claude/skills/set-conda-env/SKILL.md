---
description: Detect the right conda env for this project and configure the SessionStart hook in the project .claude/settings.json.
allowed-tools: Bash Read Write Edit
---

## Purpose

Update `.claude/settings.json` (project scope) so the `SessionStart` hook runs:

```
source <conda.sh> && conda activate <env>
```

with a visible `statusMessage` spinner.

## Input

`args` (optional): conda env name to use directly (skip detection).

## Steps

### 1. Determine the conda env

If `args` is provided and non-empty, use it directly. Skip detection.

Otherwise, read `.env` in the project root and look for lines matching
`CONDA_SH=<path>` and `CONDA_ENV=<value>`. Use those values if found.

If no env is found, run `conda env list` and ask the user to specify one. After the user specifies, write the value to `.env` as `CONDA_ENV=<env>` (add the line if absent, update if already present).

### 2. Validate the env exists

```bash
source <conda.sh> && conda env list | grep -w <env>
```

If not found, warn the user and stop.

### 3. Update `.claude/settings.json`

Target file: `<project root>/.claude/settings.json` — **not** `~/.claude/settings.json`.

Read the file. Find `hooks.SessionStart` and the entry with `matcher: "startup"`.

Update (or add) that entry's `hooks[0]` to:
```json
{
  "type": "command",
  "command": "bash",
  "args": ["-c", "source <conda.sh> && conda activate <env>"],
  "statusMessage": "conda: activating <env>..."
}
```

If no `SessionStart` block exists yet, add it to the `hooks` object:
```json
"SessionStart": [
  {
    "matcher": "startup",
    "hooks": [
      {
        "type": "command",
        "command": "bash",
        "args": ["-c", "source <conda.sh> && conda activate <env>"],
        "statusMessage": "conda: activating <env>..."
      }
    ]
  }
]
```

If no `hooks` key exists at all, add it. Preserve all other fields.

### 4. Report

- Which env was selected and how (args / detection source)
- The exact command that will run at session start
- That the change takes effect at the next session start
