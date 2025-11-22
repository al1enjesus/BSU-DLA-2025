#!/bin/bash

check_utils() {
    for util in "$@"; do
        if ! command -v "$util" &> /dev/null; then
            echo "Ошибка: Утилита '$util' не найдена. Пожалуйста, установите ее (например, 'sudo apt install procps sysstat')." >&2
            exit 1
        fi
    done
}

echo "=================================================="
echo "Шаг A: Запуск супервизора и демонстрация сигналов"
echo "=================================================="

check_utils ./src/supervisor.sh pgrep kill

./src/supervisor.sh &
SUPERVISOR_PID=$!
echo "Супервизор запущен с PID: $SUPERVISOR_PID. Наблюдайте за логами 5 секунд..."
sleep 5

echo -e "\n---> Отправляем SIGUSR1 для переключения в 'легкий' режим"
kill -SIGUSR1 $SUPERVISOR_PID
sleep 5

echo -e "\n---> Демонстрация 'graceful reload' через SIGHUP"
kill -SIGHUP $SUPERVISOR_PID
sleep 5

echo -e "\n---> Демонстрация перезапуска при аварийном падении"
WORKER_TO_KILL=$(pgrep -P $SUPERVISOR_PID | head -n 1)
if [ -n "$WORKER_TO_KILL" ]; then
    echo "Принудительно убиваем воркера с PID: $WORKER_TO_KILL"
    kill -SIGKILL $WORKER_TO_KILL
    sleep 3
    echo "Супервизор должен был его перезапустить."
else
    echo "Не удалось найти воркера для демонстрации падения."
fi

echo -e "\n---> Корректное завершение через SIGTERM"
kill -SIGTERM $SUPERVISOR_PID
wait $SUPERVISOR_PID 2>/dev/null
echo "Демонстрация Части А завершена."


echo -e "\n=================================================="
echo "Шаг B: Планирование: nice и CPU-аффинити"
echo "=================================================="

check_utils taskset pidstat renice

echo -e "\n---> Запускаем 2 процесса с одинаковым приоритетом на одном ядре"
taskset -c 0 bash -c 'for ((i=0; i<9000000; i++)); do :; done' &
PID1=$!
taskset -c 0 bash -c 'for ((i=0; i<9000000; i++)); do :; done' &
PID2=$!

echo "Процессы $PID1 и $PID2 запущены. Снимаем метрики pidstat..."
pidstat -p $PID1,$PID2 1 5 > report_b_before.txt
echo "Результаты сохранены в report_b_before.txt"

wait $PID1 $PID2

echo -e "\n---> Запускаем снова, но второму процессу ставим nice +10"
taskset -c 0 bash -c 'for ((i=0; i<9000000; i++)); do :; done' &
PID1=$!
taskset -c 0 nice -n 10 bash -c 'for ((i=0; i<9000000; i++)); do :; done' &
PID2=$!

echo "Процессы $PID1 и $PID2 (с nice +10) запущены. Снимаем метрики..."
pidstat -p $PID1,$PID2 1 5 > report_b_after.txt
echo "Результаты сохранены в report_b_after.txt"

wait $PID1 $PID2

echo "Эксперимент с nice завершен. Сравните файлы report_b_*.txt"
