#!/bin/bash
set -euo pipefail

LIB=./libsyscall_spy.so
LOG=spy.log

if [ ! -f "$LIB" ]; then
  echo "Library $LIB not found. Run: make"
  exit 1
fi

if [ $# -eq 0 ]; then
  echo "Usage: $0 <command> [args...]"
  exit 1
fi

# Очистим лог
: > "$LOG"

echo "Running with LD_PRELOAD=$LIB"
echo "Logs will be appended to $LOG"
echo "---------------------------------"

# Запускаем, перенаправляя stderr в лог (там наши логи)
LD_PRELOAD=$LIB "$@" 2>> "$LOG"

