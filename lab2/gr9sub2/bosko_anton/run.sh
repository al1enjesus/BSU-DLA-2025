#!/bin/bash
# run.sh — запуск супервизора и демонстрация сигналов

# Путь к src и конфигу
SRC_DIR="$(dirname "$0")"
CONFIG="$SRC_DIR/config.yaml"
SUPERVISOR="$SRC_DIR/src/supervisor.py"
PID_FILE="$SRC_DIR/supervisor.pid"

echo "SRC_DIR=$SRC_DIR"
echo "SUPERVISOR=$SUPERVISOR"

# Проверка существующего супервизора
if [ -f "$PID_FILE" ]; then
    OLD_PID=$(cat "$PID_FILE")
    if kill -0 $OLD_PID 2>/dev/null; then
        echo "Супервизор уже запущен с PID $OLD_PID"
        exit 1
    else
        echo "Старый PID-файл найден, удаляем..."
        rm -f "$PID_FILE"
    fi
fi

# Запуск супервизора в фоне
echo "Запускаем супервизор..."
python3 "$SUPERVISOR" "$CONFIG" &
SUP_PID=$!
echo $SUP_PID > "$PID_FILE"
echo "Супервизор запущен с PID $SUP_PID"

# Небольшая пауза, чтобы воркеры успели стартовать
sleep 2

echo
echo "Текущие воркеры:"
ps -ef | grep worker.py | grep -v grep

echo
echo "Примеры команд для сигналов:"
echo "  graceful shutdown: kill -TERM $SUP_PID"
echo "  переключить в LIGHT режим: kill -USR1 $SUP_PID"
echo "  переключить в HEAVY режим: kill -USR2 $SUP_PID"
echo "  reload конфигурации: kill -HUP $SUP_PID"

