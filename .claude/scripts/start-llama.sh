#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ENV_FILE="${PROJECT_ROOT}/.env"

[ ! -f "$ENV_FILE" ] && { echo "llama-server: .env not found, skipping"; exit 0; }

set -a; source "$ENV_FILE"; set +a

LLAMA_HOST="${LLAMA_HOST:-127.0.0.1}"
LLAMA_PORT="${LLAMA_PORT:-8080}"

if curl -sf "http://${LLAMA_HOST}:${LLAMA_PORT}/health" > /dev/null 2>&1; then
  exit 0
fi

[ ! -f "${LLAMA_MODEL}" ] && { echo "llama-server: model not found, skipping"; exit 0; }

PROJECT_ID=$(echo "$PWD" | sha256sum | cut -c1-8)
SLOT_CACHE_DIR="$HOME/.cache/llama-server/slots/$PROJECT_ID"
mkdir -p "$SLOT_CACHE_DIR"

nohup setsid "${LLAMA_SERVER}" \
  -m "${LLAMA_MODEL}" --host "$LLAMA_HOST" --port "$LLAMA_PORT" \
  --jinja --reasoning-format deepseek --reasoning-budget 8192 \
  --spec-type draft-mtp --spec-draft-n-max 3 --spec-draft-type-k q4_0 --spec-draft-type-v q4_0 \
  -ngl 999 -c 131072 --flash-attn on \
  --cache-type-k q4_0 --cache-type-v q4_0 \
  -b 512 -ub 256 --parallel 2 \
  --slot-save-path "$SLOT_CACHE_DIR" \
  </dev/null >> /tmp/llama-server.log 2>&1 &
disown $!
echo $! > /tmp/llama-server.pid

for i in $(seq 1 30); do
  sleep 2
  if curl -sf "http://${LLAMA_HOST}:${LLAMA_PORT}/health" > /dev/null 2>&1; then
    exit 0
  fi
done
echo "llama-server: startup timed out"; exit 1
