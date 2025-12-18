#!/bin/bash
# Простая демонстрация роста памяти с гарантированной работой

echo "=================================================="
echo "              РОСТ ПАМЯТИ"
echo "=================================================="

BIN_DIR="bin"
LOG_DIR="logs"
mkdir -p "$LOG_DIR"

# Сборка
make build-mem-touch

cd "$BIN_DIR"

echo "Запускаем mem_touch..."
echo "Параметры: цель 128MB, шаг 16MB"
echo ""

# Запускаем и сразу показываем PID
./mem_touch --rss-mb 128 --step-mb 16 --sleep-ms 300 2>&1 &
MEM_PID=$!
echo "mem_touch запущен с PID: $MEM_PID"

echo ""
echo "МОНИТОРИНГ ПРОЦЕССА:"
echo "PID     COMMAND    VmSize    VmRSS"
echo "----------------------------------"

# Мониторим 15 секунд
for i in {1..15}; do
    if ! ps -p $MEM_PID > /dev/null 2>&1; then
        echo " Процесс завершился"
        break
    fi
    
    # Простой и надежный способ показать процесс
    ps -p $MEM_PID -o pid,comm,vsize,rss --no-headers 2>/dev/null | while read line; do
        PID=$(echo $line | awk '{print $1}')
        VSIZE_KB=$(echo $line | awk '{print $3}')
        RSS_KB=$(echo $line | awk '{print $4}')
        VSIZE_MB=$((VSIZE_KB / 1024))
        RSS_MB=$((RSS_KB / 1024))
        echo "$PID  mem_touch  ${VSIZE_MB}MB     ${RSS_MB}MB"
    done
    
    sleep 1
done

echo ""
echo " Завершаем процесс..."
kill -TERM $MEM_PID 2>/dev/null
