#!/bin/bash
set -e

cd "$(dirname "$0")"

echo "=== Демонстрация супервизора и воркеров ==="
echo

# Сборка проекта
echo "1. Сборка проекта..."
make -C src/ clean
make -C src/

echo
echo "2. Запуск супервизора на 15 секунд..."
cd src/
timeout 15s ./supervisor ../config.ini &
SUPERVISOR_PID=$!
cd ..

sleep 2

echo
echo "3. Получение PID супервизора и воркеров..."
echo "Супервизор PID: $SUPERVISOR_PID"
WORKER_PIDS=$(pgrep -P $SUPERVISOR_PID)
echo "Воркеры PIDs: $WORKER_PIDS"

sleep 2

echo
echo "4. Анализ воркеров с помощью pstat..."
for pid in $WORKER_PIDS; do
    echo "--- Воркер PID=$pid ---"
    src/pstat $pid | head -15
    echo
done

sleep 2

echo
echo "5. Переключение в лёгкий режим (SIGUSR1)..."
kill -USR1 $SUPERVISOR_PID

sleep 3

echo
echo "6. Переключение в тяжёлый режим (SIGUSR2)..."
kill -USR2 $SUPERVISOR_PID

sleep 3

echo
echo "7. Перезагрузка конфигурации (SIGHUP)..."
kill -HUP $SUPERVISOR_PID

sleep 3

echo
echo "8. Корректное завершение (SIGTERM)..."
kill -TERM $SUPERVISOR_PID

wait $SUPERVISOR_PID

echo
echo "=== Демонстрация завершена ==="