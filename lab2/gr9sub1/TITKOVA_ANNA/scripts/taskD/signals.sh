#!/bin/bash
# Демонстрация сигналов с гарантированной работой

echo "=================================================="
echo "       СИГНАЛЫ УПРАВЛЕНИЯ ПАМЯТЬЮ"
echo "=================================================="

BIN_DIR="bin"
LOG_DIR="logs"
mkdir -p "$LOG_DIR"

cd "$BIN_DIR"

echo " Запускаем mem_touch для управления сигналами..."
echo ""

# Запускаем с выводом в терминал (не в лог)
./mem_touch --rss-mb 96 --step-mb 16 --sleep-ms 400 &
MEM_PID=$!

echo " mem_touch запущен с PID: $MEM_PID"
echo " Используйте эти команды в другом терминале:"
echo ""
echo "   Увеличить память:  kill -USR1 $MEM_PID"
echo "   Уменьшить память: kill -USR2 $MEM_PID" 
echo "   Завершить:        kill -TERM $MEM_PID"
echo ""
echo " Текущий статус процесса:"

# Показываем статус в реальном времени
while ps -p $MEM_PID > /dev/null 2>&1; do
    echo "----------------------------------------"
    date
    ps -p $MEM_PID -o pid,vsize,rss,comm --no-headers 2>/dev/null | awk '{
        vsize_mb = $2/1024; 
        rss_mb = $3/1024;
        printf "PID: %s, VmSize: %.1fMB, VmRSS: %.1fMB, Command: %s\n", $1, vsize_mb, rss_mb, $4
    }'
    echo "Ожидание сигналов... (для выхода: Ctrl+C)"
    sleep 3
done

echo ""
echo " Демонстрация завершена"
kill -TERM $MEM_PID 2>/dev/null