#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
payload="$(cat)"

if python3 -c 'import json, sys; raise SystemExit(0 if json.load(sys.stdin).get("stop_hook_active") else 1)' <<<"$payload"; then
  printf '{}\n'
  exit 0
fi

if output="$("$repo_root/.codex/hooks/run-in-conda.sh" pytest -q 2>&1)"; then
  printf '{}\n'
else
  python3 - "$output" <<'PY'
import json
import sys

print(json.dumps({
    "decision": "block",
    "reason": "Project tests failed. Fix the failures before stopping.\n\n" + sys.argv[1][-4000:],
}))
PY
fi
