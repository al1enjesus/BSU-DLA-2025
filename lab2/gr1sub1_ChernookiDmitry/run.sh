#!/bin/bash
set -euo pipefail

SUP_BIN="./bin/supervisor"
WORKER_BIN="./bin/worker"
CFG_FILE="src/config.yaml"

echo ">>> Пересобираю проект..."
make clean all

for f in "$SUP_BIN" "$WORKER_BIN" "$CFG_FILE"; do
  [[ -f "$f" ]] || { echo "Файл $f не найден, завершаюсь"; exit 1; }
done

echo ">>> Запускаю супервизор"
$SUP_BIN "$CFG_FILE" "$WORKER_BIN" &
SUP_PID=$!
echo "PID супервизора: $SUP_PID"
sleep 2

echo ">>> Проверка процессов supervisor/worker"
ps -o pid,comm,nice,psr -C supervisor,worker

if command -v pidstat >/dev/null; then
  echo ">>> Наблюдаем загрузку CPU (5 замеров по 1 сек)"
  pidstat -u -p ALL 1 5
else
  echo "Утилита pidstat отсутствует (установите sysstat)"
fi

echo ">>> Переключение режима воркеров на LIGHT"
kill -USR1 $SUP_PID
sleep 2
ps -o pid,comm,nice,psr -C supervisor,worker

echo ">>> Переключение режима воркеров на HEAVY"
kill -USR2 $SUP_PID
sleep 2
ps -o pid,comm,nice,psr -C supervisor,worker

echo ">>> Завершаю супервизор"
kill -TERM $SUP_PID
wait $SUP_PID || true

echo ">>> Скрипт демонстрации завершён"