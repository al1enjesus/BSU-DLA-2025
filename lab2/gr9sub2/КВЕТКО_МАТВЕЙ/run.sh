#!/bin/bash

# Скрипт запуска и демонстрации супервизора

CONFIG_FILE="config.json"
SUPERVISOR_PID=""

# Определяем команду Python
if command -v python3 &> /dev/null; then
    PYTHON_CMD="python3"
elif command -v python &> /dev/null; then
    PYTHON_CMD="python"
else
    echo "Error: Python not found. Please install Python 3.6 or higher."
    exit 1
fi

echo "Using Python command: $PYTHON_CMD"

# Функция для ожидания запуска супервизора
wait_for_supervisor() {
    echo "Waiting for supervisor to start..."
    sleep 2
    SUPERVISOR_PID=$(pgrep -f "$PYTHON_CMD.*supervisor.py")
    if [ -z "$SUPERVISOR_PID" ]; then
        echo "Error: Supervisor failed to start"
        exit 1
    fi
    echo "Supervisor started with PID: $SUPERVISOR_PID"
}

# Функция отправки сигнала
send_signal() {
    local signal=$1
    local description=$2

    echo ""
    echo "=== Sending $signal: $description ==="
    kill -$signal $SUPERVISOR_PID
    sleep 2
}

# Проверка существования конфигурационного файла
if [ ! -f "$CONFIG_FILE" ]; then
    echo "Error: Config file $CONFIG_FILE not found"
    echo "Creating default config..."
    cat > $CONFIG_FILE << EOF
{
    "workers": 3,
    "mode_heavy": {
        "work_us": 9000,
        "sleep_us": 1000
    },
    "mode_light": {
        "work_us": 2000,
        "sleep_us": 8000
    }
}
EOF
    echo "Default config created: $CONFIG_FILE"
fi

# Запуск супервизора
echo "Starting supervisor with config: $CONFIG_FILE"
cd src
$PYTHON_CMD supervisor.py ../$CONFIG_FILE &
cd ..

wait_for_supervisor

# Демонстрация работы
echo ""
echo "=== Demonstration started ==="
echo "Supervisor and workers are running..."
echo "Check the output above to see worker activity."

# Даем поработать несколько секунд
sleep 5

# Демонстрация переключения режимов
send_signal "USR1" "Switch workers to LIGHT mode"
sleep 5

send_signal "USR2" "Switch workers to HEAVY mode" 
sleep 5

# Демонстрация перезагрузки
echo ""
echo "=== Modifying config file for reload demonstration ==="
# Создаем новую конфигурацию с другим количеством воркеров
cat > config_reload.json << EOF
{
    "workers": 2,
    "mode_heavy": {
        "work_us": 7000,
        "sleep_us": 3000
    },
    "mode_light": {
        "work_us": 1000,
        "sleep_us": 9000
    }
}
EOF

# Копируем новую конфигурацию поверх старой
cp config_reload.json config.json
rm config_reload.json

echo "Config file updated - now sending HUP signal"
send_signal "HUP" "Graceful reload"
sleep 5

# Демонстрация корректного завершения
send_signal "TERM" "Graceful shutdown"

# Ожидание завершения
echo ""
echo "Waiting for shutdown to complete..."
wait $SUPERVISOR_PID 2>/dev/null
echo "=== Demonstration completed ==="
