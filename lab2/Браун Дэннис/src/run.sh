#!/bin/bash

# Скрипт демонстрации для супервизора

echo "=== Lab 2 Supervisor Demo ==="
echo ""

# Переходим в директорию скрипта
cd "$(dirname "$0")"

# Проверяем, что файлы существуют
if [ ! -f "supervisor.py" ] || [ ! -f "worker.py" ] || [ ! -f "config.json" ]; then
    echo "Error: Required files not found!"
    exit 1
fi

echo "Starting supervisor with config.json..."
python3 supervisor.py config.json &
SUPERVISOR_PID=$!

echo "Supervisor started with PID: $SUPERVISOR_PID"
echo "Waiting 3 seconds for workers to start..."
sleep 3

echo ""
echo "=== Demo 1: Switching to LIGHT mode (SIGUSR1) ==="
kill -USR1 $SUPERVISOR_PID
sleep 5

echo ""
echo "=== Demo 2: Switching to HEAVY mode (SIGUSR2) ==="
kill -USR2 $SUPERVISOR_PID
sleep 5

echo ""
echo "=== Demo 3: Graceful reload (SIGHUP) ==="
kill -HUP $SUPERVISOR_PID
sleep 5

echo ""
echo "=== Demo 4: Simulating worker crash ==="
# Найдем PID первого воркера и "убьем" его
WORKER_PID=$(ps aux | grep "worker.py worker_1" | grep -v grep | awk '{print $2}')
if [ ! -z "$WORKER_PID" ]; then
    echo "Killing worker with PID: $WORKER_PID"
    kill -9 $WORKER_PID
    sleep 3
fi

echo ""
echo "=== Demo 5: Graceful shutdown (SIGTERM) ==="
kill -TERM $SUPERVISOR_PID

# Ждем завершения
wait $SUPERVISOR_PID
echo ""
echo "=== Demo completed ==="
