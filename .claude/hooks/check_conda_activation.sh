#!/bin/bash
# PreToolUse hook: conda 활성화 패턴 강제

input=$(cat)
cmd=$(echo "$input" | python3 -c "
import sys, json
try:
    d = json.load(sys.stdin)
    print(d.get('tool_input', {}).get('command', ''))
except:
    print('')
" 2>/dev/null)

# conda activate without source prefix
if echo "$cmd" | grep -q "conda activate" && ! echo "$cmd" | grep -q "source.*conda\.sh"; then
    echo "올바른 conda 활성화 패턴을 사용하세요:"
    echo "  source /home/seuoh/miniforge3/etc/profile.d/conda.sh && conda activate <env> && <command>"
    exit 2
fi

# conda run -n pattern
if echo "$cmd" | grep -qE "conda run (-n|--name)"; then
    echo "conda run 대신 올바른 패턴을 사용하세요:"
    echo "  source /home/seuoh/miniforge3/etc/profile.d/conda.sh && conda activate <env> && <command>"
    exit 2
fi

exit 0