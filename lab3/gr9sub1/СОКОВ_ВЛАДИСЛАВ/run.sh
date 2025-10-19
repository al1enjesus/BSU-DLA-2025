#!/bin/bash
# Простая обёртка для запуска pstat
if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <pid>"
  exit 1
fi
PID="$1"
python3 "$(dirname "$0")/src/pstat.py" "$PID"