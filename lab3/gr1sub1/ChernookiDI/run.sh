#!/bin/bash
set -e

echo "start"
echo

# Сборка утилиты
echo "1. Сборка"
make clean
make
echo "✓ Утилита собрана"
echo

# Получаем PID текущего процесса bash
CURRENT_PID=$$
echo "2. Демонстрация на PID текущего процесса: $CURRENT_PID"
echo

echo "=== Наша утилита pstat ==="
./pstat $CURRENT_PID
echo

echo "=== Сравнение с системными утилитами ==="
echo
echo "--- ps aux для процесса $CURRENT_PID ---"
ps aux | head -1
ps aux | grep $CURRENT_PID | grep -v grep
echo

echo "--- top -p $CURRENT_PID (1 итерация) ---"
timeout 2 top -p $CURRENT_PID -n 1 -b | grep -E "(PID|$CURRENT_PID)"
echo

echo "--- pidstat -p $CURRENT_PID 1 1 ---"
if command -v pidstat &> /dev/null; then
    pidstat -p $CURRENT_PID 1 1
else
    echo "pidstat не установлен (установите sysstat: sudo apt install sysstat)"
fi
echo

echo "=== Тестирование с другими процессами ==="
echo
echo "--- Процесс init (PID 1) ---"
./pstat 1 || echo "Возможно, нужны дополнительные права для доступа к /proc/1"
echo

echo "--- Случайный процесс из системы ---"
RANDOM_PID=$(ps -eo pid --no-headers | head -10 | tail -1 | tr -d ' ')
if [ ! -z "$RANDOM_PID" ]; then
    echo "Тестируем PID: $RANDOM_PID"
    ./pstat $RANDOM_PID || echo "Не удалось получить информацию о PID $RANDOM_PID"
fi
echo

echo "=== Дополнительные тесты (E*) ==="
if command -v strace &> /dev/null; then
    echo "--- strace доступен, демонстрация strace -c ---"
    echo "Запускаем: timeout 3 strace -c -p $CURRENT_PID"
    timeout 3 strace -c -p $CURRENT_PID 2>&1 | head -20 || echo "strace завершён"
else
    echo "strace не установлен"
fi
echo

if command -v perf &> /dev/null; then
    echo "--- perf доступен, демонстрация perf stat ---"
    echo "Запускаем: timeout 2 perf stat -p $CURRENT_PID sleep 2"
    timeout 3 perf stat -p $CURRENT_PID sleep 2 2>&1 || echo "perf завершён или недоступен"
else
    echo "perf не установлен"
fi
echo

echo "=== Демонстрация завершена ==="
echo "Все файлы находятся в $(pwd)"
echo "Для воспроизведения запустите: ./run.sh"