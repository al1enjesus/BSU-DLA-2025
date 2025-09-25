#!/bin/bash
# Демонстрация работы супервизора

SUPERVISOR_PATH="./supervisor.py"

# Проверка наличия файла
if [ ! -f "$SUPERVISOR_PATH" ]; then
  echo "[Error] Не найден файл supervisor.py по пути $SUPERVISOR_PATH"
  exit 1
fi

echo "[Run] Запуск супервизора..."
python3 "$SUPERVISOR_PATH" &
SUP_PID=$!

# Дать время на запуск
sleep 2

echo "[Run] Супервизор запущен с PID=$SUP_PID"
echo "------ Текущие процессы supervisor/worker ------"
ps -f --ppid $SUP_PID

# Демонстрация перезапуска
echo "[Run] Отправляем SIGHUP (перезапуск воркеров)..."
kill -HUP $SUP_PID
sleep 2
echo "------ После перезапуска ------"
ps -f --ppid $SUP_PID

# Корректное завершение
echo "[Run] Отправляем SIGTERM (завершение)..."
kill -TERM $SUP_PID
wait $SUP_PID

echo "[Run] Супервизор завершил работу."
