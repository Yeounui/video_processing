#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
env_file="$repo_root/.env"

env_name="$(grep -m1 '^CONDA_ENV=' "$env_file" 2>/dev/null | cut -d= -f2- | tr -d '[:space:]')"

if [[ -z "$env_name" ]]; then
  printf 'Configure the project Conda environment with `$set-conda-env` before running project commands.\n'
  exit 0
fi

printf 'Run project commands through `.codex/hooks/run-in-conda.sh`. The configured Conda environment is `%s`.\n' "$env_name"
