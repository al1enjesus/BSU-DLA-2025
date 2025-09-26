#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BUILD_DIR="$SCRIPT_DIR"

echo "Build..."
make -C "$SRC_DIR"

# Копируем бинарники в основную папку
cp "$SRC_DIR"/{supervisor,worker,mem_touch} "$BUILD_DIR/" 2>/dev/null || true

# ensure config exists
if [ ! -f config.json ]; then
  cat > config.json <<'EOF'
{
  "workers": 4,
  "mode_heavy": { "work_us": 9000, "sleep_us": 1000 },
  "mode_light": { "work_us": 2000, "sleep_us": 8000 },
  "restart_limit": { "count": 5, "window_s": 30 },
  "nice": { "high": 0, "low": 10 }
}
EOF
fi

echo "Starting supervisor..."
./supervisor &
SUP_PID=$!
echo "supervisor pid=$SUP_PID"
sleep 1

echo "List processes:"
ps -o pid,comm,ni -p $SUP_PID -L

echo "Send SIGUSR2 (switch to hard):"
kill -USR2 $SUP_PID
sleep 3

echo "Collect pidstat for 5 seconds (requires sysstat/pidstat)"
if command -v pidstat >/dev/null 2>&1; then
  pidstat -u 1 5
else
  echo "pidstat not found, skipping"
fi

echo "Send SIGUSR1 (switch to light):"
kill -USR1 $SUP_PID
sleep 3

echo "Collect pidstat for 5 seconds (requires sysstat/pidstat)"
if command -v pidstat >/dev/null 2>&1; then
  pidstat -u 1 5
else
  echo "pidstat not found, skipping"
fi

echo "Trigger reload (SIGHUP)"
kill -HUP $SUP_PID
sleep 3

echo "Collect pidstat for 5 seconds (requires sysstat/pidstat)"
if command -v pidstat >/dev/null 2>&1; then
  pidstat -u 1 5
else
  echo "pidstat not found, skipping"
fi

echo "Now graceful stop (SIGTERM)"
kill -TERM $SUP_PID
wait $SUP_PID 2>/dev/null || true
echo "Supervisor stopped"

echo "=== B. Планирование: nice и CPU-affinity ==="
echo "[*] Запускаем два процесса worker с разными приоритетами..."
taskset -c 0 nice -n 0 ./worker --duration 5 &
PID1=$!
taskset -c 0 nice -n 10 ./worker --duration 5 &
PID2=$!

echo "[*] Смотрим распределение CPU (ожидаем, что nice +10 получит меньше CPU):"
pidstat -u -p $PID1,$PID2 1 5

kill -TERM $PID1 $PID2
wait $PID1 $PID2 || true
echo

echo "[*] Демонстрация завершена."

