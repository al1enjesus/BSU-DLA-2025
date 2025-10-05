#!/bin/bash

echo "=================================================="
echo "Шаг A: Запуск супервизора и демонстрация сигналов"
echo "=================================================="

# Запускаем супервизор в фоне
./src/supervisor.sh &
SUPERVISOR_PID=$!
echo "Супервизор запущен с PID: $SUPERVISOR_PID. Наблюдайте за логами..."
sleep 3

echo -e "\n---> Отправляем SIGUSR1 для переключения в 'легкий' режим"
kill -SIGUSR1 $SUPERVISOR_PID
sleep 5

echo -e "\n---> Отправляем SIGUSR2 для возврата в 'тяжелый' режим"
kill -SIGUSR2 $SUPERVISOR_PID
sleep 5

echo -e "\n---> Демонстрация 'graceful reload' через SIGHUP"
kill -SIGHUP $SUPERVISOR_PID
sleep 5

echo -e "\n---> Демонстрация перезапуска при аварийном падении"
# Убиваем первого воркера (находим его PID через pgrep)
WORKER_TO_KILL=$(pgrep -P $SUPERVISOR_PID | head -n 1)
echo "Принудительно убиваем воркера с PID: $WORKER_TO_KILL"
kill -SIGKILL $WORKER_TO_KILL
sleep 3
echo "Супервизор должен был его перезапустить."

echo -e "\n---> Корректное завершение через SIGTERM"
kill -SIGTERM $SUPERVISOR_PID
# Ждем, пока процесс супервизора действительно завершится
wait $SUPERVISOR_PID
echo "Демонстрация завершена."


echo "=================================================="
echo "Шаг B: Планирование: nice и CPU-аффинити"
echo "=================================================="

echo -e "\n---> Запускаем 2 процесса с одинаковым приоритетом на одном ядре"
# taskset -c 0 привязывает процесс к ядру №0
taskset -c 0 bash -c 'for ((i=0; i<9000000; i++)); do :; done' &
PID1=$!
taskset -c 0 bash -c 'for ((i=0; i<9000000; i++)); do :; done' &
PID2=$!

echo "Процессы $PID1 и $PID2 запущены. Снимаем метрики pidstat..."
# Снимаем статистику 5 раз с интервалом 1 секунду
pidstat -p $PID1,$PID2 1 5 > report_b_before.txt
echo "Результаты сохранены в report_b_before.txt"

wait $PID1 $PID2

echo -e "\n---> Запускаем снова, но второму процессу ставим nice +10"
taskset -c 0 bash -c 'for ((i=0; i<9000000; i++)); do :; done' &
PID1=$!
taskset -c 0 renice -n 10 -p $$ >/dev/null && bash -c 'for ((i=0; i<9000000; i++)); do :; done' &
PID2=$!

echo "Процессы $PID1 и $PID2 (с nice +10) запущены. Снимаем метрики..."
pidstat -p $PID1,$PID2 1 5 > report_b_after.txt
echo "Результаты сохранены в report_b_after.txt"

wait $PID1 $PID2

echo "Эксперимент с nice завершен. Сравните файлы report_b_*.txt"
