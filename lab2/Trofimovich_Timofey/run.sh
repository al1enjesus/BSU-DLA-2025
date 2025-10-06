#!/bin/bash

set -e

cd "$(dirname "$0")"

echo "Building supervisor and worker..."
if ! make -C src; then
    echo "Build failed"
    exit 1
fi

echo "Starting supervisor..."
cd src
./supervisor &
SUPERVISOR_PID=$!

echo "Supervisor started with PID: $SUPERVISOR_PID"
echo ""

# Даем время на запуск воркеров
sleep 2

echo "=== Demonstration ==="

# Более надежный способ найти PID воркера
find_worker_pid() {
    # Ищем процессы worker, которые являются прямыми потомками супервизора
    ps -o pid,ppid,command | awk -v ppid="$SUPERVISOR_PID" '$2 == ppid && $3 ~ /worker/ {print $1; exit}'
}

echo "1. Sending SIGUSR1 to switch workers to LIGHT mode"
kill -USR1 $SUPERVISOR_PID
sleep 3

echo ""
echo "2. Sending SIGUSR2 to switch workers to HEAVY mode"
kill -USR2 $SUPERVISOR_PID
sleep 3

echo ""
echo "3. Sending SIGHUP to reload workers"
kill -HUP $SUPERVISOR_PID
sleep 3

echo ""
echo "4. Testing worker restart (gracefully terminating one worker)"
WORKER_PID=$(find_worker_pid)
if [ ! -z "$WORKER_PID" ] && [ "$WORKER_PID" != "PID" ]; then
    echo "Gracefully terminating worker $WORKER_PID"
    kill -TERM $WORKER_PID
    sleep 2
else
    echo "Could not find worker process"
fi

echo ""
echo "5. Graceful shutdown with SIGTERM"
kill -TERM $SUPERVISOR_PID

# Ждем завершения
wait $SUPERVISOR_PID 2>/dev/null || true

echo ""
echo "Demo completed!"