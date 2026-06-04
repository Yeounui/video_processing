#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ENV_FILE="${PROJECT_ROOT}/.env"

if [ ! -f "$ENV_FILE" ]; then
  echo "conda: .env not found at ${ENV_FILE}, skipping"
  exit 0
fi

set -a; source "$ENV_FILE"; set +a

# Inject all .env vars into the Claude Code session via CLAUDE_ENV_FILE
if [ -n "${CLAUDE_ENV_FILE:-}" ]; then
  while IFS= read -r line; do
    [[ "$line" =~ ^[[:space:]]*# ]] && continue
    [[ -z "${line//[[:space:]]/}" ]] && continue
    echo "export $line" >> "$CLAUDE_ENV_FILE"
  done < "$ENV_FILE"
fi

source "${CONDA_SH}"
conda activate "${CONDA_ENV}"
echo "conda: activated ${CONDA_ENV}"
