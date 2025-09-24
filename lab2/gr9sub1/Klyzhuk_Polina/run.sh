# Создаем виртуальное окружение если нужно
if [ ! -d "venv" ]; then
    python3 -m venv venv
fi
source venv/bin/activate

# Запуск супервизора в фоне
echo "Запуск супервизора..."
python src/supervisor.py config.json &
SUPERVISOR_PID=$!

echo "Супервизор запущен с PID: $SUPERVISOR_PID"
sleep 2

# Демонстрация различных сигналов
echo -e "\n1. Демонстрация SIGUSR1 (переключение в легкий режим)"
kill -USR1 $SUPERVISOR_PID
sleep 3

echo -e "\n2. Демонстрация SIGUSR2 (переключение в тяжелый режим)"
kill -USR2 $SUPERVISOR_PID
sleep 3

echo -e "\n3. Демонстрация SIGHUP (graceful reload)"
kill -HUP $SUPERVISOR_PID
sleep 3

echo -e "\n4. Демонстрация завершения одного воркера"
WORKER_PID=$(ps aux | grep "worker.py" | grep -v grep | head -1 | awk '{print $2}')
if [ ! -z "$WORKER_PID" ]; then
    echo "Завершаем воркер с PID: $WORKER_PID"
    kill -TERM $WORKER_PID
    sleep 2
fi

echo -e "\n5. Демонстрация graceful shutdown"
kill -TERM $SUPERVISOR_PID

# Ожидание завершения
wait $SUPERVISOR_PID
echo -e "\nДемонстрация завершена"