#!/bin/bash

echo "=== Лабораторная работа 2 - Задание A ==="
echo "Демонстрация супервизора с воркерами"

# Переходим в папку src
cd src

echo ""
echo "1. Проверяем файлы конфигурации..."
cat config.json

echo ""
echo "2. Запускаем супервизор..."
python3 supervisor.py config.json &

# Запоминаем PID супервизора
SUPERVISOR_PID=$!
echo "Супервизор запущен с PID: $SUPERVISOR_PID"

# Даём время на запуск воркеров
echo ""
echo "3. Ждём запуска воркеров (3 секунды)..."
sleep 3

echo ""
echo "4. Проверяем запущенные процессы:"
ps aux | grep -E "(supervisor|worker).py" | grep -v grep

echo ""
echo "5. Демонстрация работы воркеров (5 секунд)..."
sleep 5

echo ""
echo "6. Переключение в лёгкий режим (SIGUSR1)..."
kill -USR1 $SUPERVISOR_PID
sleep 3

echo ""
echo "7. Переключение в тяжёлый режим (SIGUSR2)..."
kill -USR2 $SUPERVISOR_PID
sleep 3

echo ""
echo "8. Graceful reload конфигурации (SIGHUP)..."
kill -HUP $SUPERVISOR_PID
sleep 3

echo ""
echo "9. Имитация падения воркера..."
# Находим PID первого воркера
WORKER_PID=$(ps aux | grep worker.py | grep -v grep | head -1 | awk '{print $2}')
if [ -n "$WORKER_PID" ]; then
    echo "Принудительно завершаем воркер с PID: $WORKER_PID"
    kill -9 $WORKER_PID
    sleep 2
    echo "Проверяем перезапуск:"
    ps aux | grep worker.py | grep -v grep
else
    echo "Воркеры не найдены"
fi

echo ""
echo "10. Graceful shutdown (SIGTERM)..."
kill -TERM $SUPERVISOR_PID

# Ждём завершения
sleep 2

echo ""
echo "11. Проверяем завершение всех процессов:"
ps aux | grep -E "(supervisor|worker).py" | grep -v grep

if [ $? -eq 0 ]; then
    echo "Предупреждение: остались процессы, принудительно завершаем..."
    pkill -f "supervisor.py"
    pkill -f "worker.py"
    sleep 1
else
    echo "Все процессы корректно завершены"
fi

echo ""
echo "=== Демонстрация задания A завершена ==="