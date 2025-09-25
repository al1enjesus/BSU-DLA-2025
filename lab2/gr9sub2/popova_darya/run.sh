#!/usr/bin/env bash
# run.sh — example background runner, supports supervisor in src/
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

CONFIG="config.json"
WORKERS=3
LOG="supervisor.log"
PIDFILE="supervisor.pid"
SUPERVISOR_PATH="src/supervisor.py"   # <- изменено: путь к файлу в src/

if ! command -v python3 >/dev/null 2>&1; then
  echo "ERROR: python3 not found in PATH" >&2
  exit 2
fi

if [ ! -f "$SUPERVISOR_PATH" ]; then
  echo "ERROR: supervisor not found at $SUPERVISOR_PATH" >&2
  exit 3
fi

echo "[run.sh] starting supervisor (path=${SUPERVISOR_PATH}, config=${CONFIG}, workers=${WORKERS})"
nohup python3 "$SUPERVISOR_PATH" --config "$CONFIG" --workers "$WORKERS" > "$LOG" 2>&1 &

SUP_PID=$!
echo "$SUP_PID" > "$PIDFILE"

echo "[run.sh] supervisor started with pid=$SUP_PID"
echo "[run.sh] logs -> $DIR/$LOG"
echo "[run.sh] pid file -> $DIR/$PIDFILE"
echo
echo "Команды для управления (в другом терминале):"
echo "  kill -SIGHUP $SUP_PID    # reload"
echo "  kill -SIGUSR1 $SUP_PID   # light"
echo "  kill -SIGUSR2 $SUP_PID   # heavy"
echo "  kill -SIGTERM $SUP_PID   # graceful shutdown"
echo "  tail -f $LOG"
echo "  kill -SIGKILL $SUP_PID   # force stop"
