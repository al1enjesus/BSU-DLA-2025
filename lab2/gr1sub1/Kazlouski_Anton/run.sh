set -e

cd "$(dirname "$0")"

echo "Building supervisor and worker..."
make -C src

echo "Starting supervisor..."
cd src
./supervisor &
SUPERVISOR_PID=$!

echo "Supervisor started with PID: $SUPERVISOR_PID"
echo ""

# Даем время на запуск воркеров
sleep 2

echo "=== Demonstration ==="
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
echo "4. Testing worker restart (killing one worker)"
WORKER_PID=$(ps aux | grep -v grep | grep "./worker" | head -1 | awk '{print $2}')
if [ ! -z "$WORKER_PID" ]; then
    echo "Killing worker $WORKER_PID"
    kill -9 $WORKER_PID
    sleep 2
fi

echo ""
echo "5. Graceful shutdown with SIGTERM"
kill -TERM $SUPERVISOR_PID

# Ждем завершения
wait $SUPERVISOR_PID 2>/dev/null || true

echo ""
echo "Demo completed!"