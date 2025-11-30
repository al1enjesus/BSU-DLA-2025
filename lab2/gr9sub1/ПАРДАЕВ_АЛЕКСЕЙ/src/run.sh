#!/usr/bin/env bash
set -euo pipefail

CONFIG=config.json

python3 supervisor.py -c "$CONFIG" -n 4 &
SUP_PID=$!
echo "Supervisor pid: $SUP_PID"
sleep 1

echo "Посмотрим процессы:"
ps -o pid,ppid,ni,psr,cmd -C python3 | sed -n '1,200p'

# переключаем в легкий режим
echo "Broadcast LIGHT (SIGUSR1)"
kill -USR1 "$SUP_PID"
sleep 2

# переключаем обратно в тяжелый
echo "Broadcast HEAVY (SIGUSR2)"
kill -USR2 "$SUP_PID"
sleep 2

# показать pidstat (если установлен)
if command -v pidstat >/dev/null 2>&1; then
  echo "pidstat sample (2s x 5)"
  pidstat -u 2 5
else
  echo "pidstat not installed, skipping"
fi

# graceful reload (редактируйте config.json между командами, затем:)
echo "SIGHUP -> reload config"
kill -HUP "$SUP_PID"
sleep 3

# graceful shutdown
echo "Sending TERM to supervisor for graceful shutdown"
kill -TERM "$SUP_PID"
wait "$SUP_PID" || true
echo "Supervisor exited"
