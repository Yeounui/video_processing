#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
env_file="$repo_root/.env"

env_name="$(grep -m1 '^CONDA_ENV=' "$env_file" 2>/dev/null | cut -d= -f2- | tr -d '[:space:]')"

if [[ -z "$env_name" ]]; then
  printf 'CONDA_ENV not set in %s\n' "$env_file" >&2
  exit 1
fi

exec conda run -n "$env_name" -- "$@"
