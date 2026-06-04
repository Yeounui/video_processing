#!/usr/bin/env bash

PID_FILE="/tmp/llama-server.pid"
[ ! -f "$PID_FILE" ] && exit 0

PID=$(cat "$PID_FILE")
kill "$PID" 2>/dev/null || true

# Wait for graceful shutdown so slot cache is written
for i in $(seq 1 10); do
  sleep 1
  kill -0 "$PID" 2>/dev/null || break
done

rm -f "$PID_FILE"
