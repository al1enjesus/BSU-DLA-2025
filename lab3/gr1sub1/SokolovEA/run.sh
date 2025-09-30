#!/bin/bash

# Скрипт для демонстрации утилиты pstat и сравнения с системными утилитами

set -e

echo "=== Lab 3: pstat утилита и диагностика процессов ==="
echo

# Переходим в папку с исходниками
cd "$(dirname "$0")/src"

# Собираем утилиту
echo "Сборка утилиты pstat..."
make clean > /dev/null 2>&1 || true
make all

echo "✓ Утилита собрана успешно!"
echo

# Получаем PID текущего процесса bash
BASH_PID=$$
echo "Демонстрация работы с PID текущего bash процесса: $BASH_PID"
echo

# Запускаем нашу утилиту
echo "=== Вывод pstat ==="
../bin/pstat $BASH_PID
echo

# Сравниваем с системными утилитами
echo "=== Сравнение с ps ==="
ps -p $BASH_PID -o pid,ppid,state,nlwp,utime,stime,rss,vsz,cmd
echo

echo "=== Сравнение с top (одна итерация) ==="
top -p $BASH_PID -n 1 -b | grep -A 5 "PID"
echo

# Если доступен pidstat
if command -v pidstat >/dev/null 2>&1; then
    echo "=== Сравнение с pidstat ==="
    pidstat -p $BASH_PID 1 1
    echo
fi

# Демонстрация на другом процессе (init)
echo "=== Демонстрация на процессе init (PID 1) ==="
../bin/pstat 1
echo

echo "=== Демонстрация strace (если доступен) ==="
if command -v strace >/dev/null 2>&1; then
    echo "Запуск strace для анализа системных вызовов нашей утилиты:"
    echo "strace -c ../bin/pstat $BASH_PID"
    strace -c ../bin/pstat $BASH_PID 2>&1 | tail -20
else
    echo "strace не доступен в системе"
fi
echo

echo "=== Демонстрация perf (если доступен) ==="
if command -v perf >/dev/null 2>&1; then
    echo "Анализ производительности с помощью perf:"
    echo "perf stat ../bin/pstat $BASH_PID"
    perf stat ../bin/pstat $BASH_PID 2>&1 || echo "perf может требовать дополнительных прав"
else
    echo "perf не доступен в системе"
fi

echo
echo "Демонстрация завершена!"
echo "Исполняемый файл: $(pwd)/bin/pstat"