#!/bin/bash
# PreToolUse hook: conda 활성화 패턴 강제

cmd=$(python3 -c "
import sys, json
try:
    print(json.load(sys.stdin).get('tool_input', {}).get('command', ''))
except Exception:
    pass
")

# conda activate without source prefix
if echo "$cmd" | grep -q "conda activate" && \
   ! echo "$cmd" | grep -qE "(source|\.)[[:space:]]+\"?(\\\$CONDA_SH|.*conda\.sh)"; then
    echo "올바른 conda 활성화 패턴을 사용하세요:"
    echo "  source \$CONDA_SH && conda activate <env> && <command>"
    exit 2
fi

# conda run -n pattern
if echo "$cmd" | grep -qE "conda run (-n|--name)"; then
    echo "conda run 대신 올바른 패턴을 사용하세요:"
    echo "  source \$CONDA_SH && conda activate <env> && <command>"
    exit 2
fi

exit 0