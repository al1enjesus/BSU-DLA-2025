#!/bin/bash
set -e

echo "=== Сборка ==="
make clean all

SUP_BIN="./bin/supervisor"
WORKER_BIN="./bin/worker"
CONFIG="src/config.yaml"

# Проверка файлов
if [[ ! -f "$SUP_BIN" ]]; then
    echo "Ошибка: супервизор $SUP_BIN не найден"
    exit 1
fi
if [[ ! -f "$WORKER_BIN" ]]; then
    echo "Ошибка: воркер $WORKER_BIN не найден"
    exit 1
fi
if [[ ! -f "$CONFIG" ]]; then
    echo "Ошибка: конфиг $CONFIG не найден"
    exit 1
fi

echo "=== Запуск супервизора с воркерами ==="
$SUP_BIN $CONFIG $WORKER_BIN &
SUP_PID=$!
echo "[RUN] Supervisor PID=$SUP_PID"
sleep 2

echo "=== Статус процессов ==="
ps -o pid,comm,nice,psr -C supervisor,worker

echo "=== CPU статистика (pidstat 1 5) ==="
if command -v pidstat >/dev/null 2>&1; then
    pidstat -u -p ALL 1 5
else
    echo "pidstat не найден, используйте 'sudo apt install sysstat'"
fi

echo "=== Переключение режима воркеров в LIGHT ==="
kill -USR1 $SUP_PID
sleep 2
ps -o pid,comm,nice,psr -C supervisor,worker

echo "=== Переключение режима воркеров в HEAVY ==="
kill -USR2 $SUP_PID
sleep 2
ps -o pid,comm,nice,psr -C supervisor,worker

echo "=== Завершение супервизора ==="
kill -TERM $SUP_PID
wait $SUP_PID || true

echo "=== Конец демонстрации ==="
