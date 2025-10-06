#!/bin/bash


echo "=== Дополнительные эксперименты ==="
echo

echo "1. Создание тестового процесса с нагрузкой"
(sleep 5 & echo $! > test_pid.tmp; while [ -f test_pid.tmp ]; do echo >/dev/null; done) &
TEST_PID=$!
echo "Создан процесс PID: $TEST_PID"
sleep 1

echo
echo "=== Анализ тестового процесса ==="
./pstat $TEST_PID
echo

echo "=== Сравнение с ps ==="
ps aux | head -1
ps aux | grep $TEST_PID | grep -v grep
echo

kill $TEST_PID 2>/dev/null
rm -f test_pid.tmp

echo "2. Анализ различных процессов системы"
echo

PROCS=$(ps -eo pid,comm --no-headers | head -10 | tail -5)
echo "Найденные процессы:"
echo "$PROCS"
echo

for line in $PROCS; do
    if [[ $line =~ ^[0-9]+$ ]]; then
        PID=$line
        echo "--- Анализ PID $PID ---"
        ./pstat $PID 2>/dev/null || echo "Недоступен для анализа"
        echo
    fi
done

echo "3. Демонстрация форматирования чисел"
echo

echo "Выделяем много памяти."
python3 -c "
import time
data = [0] * (50 * 1024 * 1024 // 8)
print('Memory allocated, PID:', $(echo $$))
time.sleep(3)
" &
MEM_PID=$!
sleep 1

echo "Анализ процесса (PID: $MEM_PID):"
./pstat $MEM_PID 2>/dev/null || echo "Процесс завершился"

kill $MEM_PID 2>/dev/null
wait $MEM_PID 2>/dev/null

echo
echo "4. Проверка доступности файлов /proc..."
echo

echo "Проверяем доступность различных файлов /proc для PID $$:"
for file in stat status io smaps_rollup; do
    path="/proc/$$/file"
    if [ -r "/proc/$$/$file" ]; then
        echo "✓ /proc/$$/$file - доступен"
    else
        echo "✗ /proc/$$/$file - недоступен"
    fi
done
