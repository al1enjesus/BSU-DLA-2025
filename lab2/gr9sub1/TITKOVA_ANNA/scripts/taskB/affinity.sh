#!/bin/bash
# Демонстрация CPU affinity с анализом

echo "=================================================="
echo "               CPU AFFINITY"
echo "=================================================="

BIN_DIR="bin"
LOG_DIR="logs"
mkdir -p "$LOG_DIR"

cleanup() {
    pkill -f "./cpu_burn" 2>/dev/null || true
    sleep 1
}

trap cleanup EXIT INT TERM

# Проверяем CPU
CPU_COUNT=$(nproc)
echo "Обнаружено CPU ядер: $CPU_COUNT"

if [ $CPU_COUNT -lt 2 ]; then
    echo "Нужно минимум 2 CPU ядра для демонстрации affinity"
    exit 1
fi

cd "$BIN_DIR"

echo "Запускаем процессы на разных CPU ядрах..."
echo ""

# Запускаем процессы
./cpu_burn --work-us 8000 --sleep-us 2000 --duration 20 --cpu 0 > "../$LOG_DIR/affinity_cpu0.log" 2>&1 &
PID1=$!
echo "Процесс 1: PID $PID1, CPU 0"

./cpu_burn --work-us 8000 --sleep-us 2000 --duration 20 --cpu 1 > "../$LOG_DIR/affinity_cpu1.log" 2>&1 &
PID2=$!
echo "Процесс 2: PID $PID2, CPU 1"

echo ""
echo "=== СБОР МЕТРИК ==="
echo "Собираем информацию о распределении по ядрам..."
echo "Время     PID    CPU %CPU" > "../$LOG_DIR/affinity_metrics.txt"

for i in {1..15}; do
    TIMESTAMP=$(date +%H:%M:%S)
    ps -p $PID1,$PID2 -o pid,psr,pcpu 2>/dev/null | tail -2 | while read line; do
        if [ -n "$line" ]; then
            echo "$TIMESTAMP $line" >> "../$LOG_DIR/affinity_metrics.txt"
        fi
    done
    echo "Снимок $i/15..."
    sleep 1
done

echo ""
echo "Ожидаем завершения процессов..."
wait $PID1 $PID2 2>/dev/null

echo ""
echo "=== АНАЛИЗ РЕЗУЛЬТАТОВ ==="
echo ""

# Анализ распределения по ядрам
echo "РАСПРЕДЕЛЕНИЕ ПО CPU ЯДРАМ:"
CORE0_COUNT=$(grep " $PID1 " "../$LOG_DIR/affinity_metrics.txt" 2>/dev/null | awk '{print $3}' | grep -c "^0$" || echo "0")
CORE1_COUNT=$(grep " $PID2 " "../$LOG_DIR/affinity_metrics.txt" 2>/dev/null | awk '{print $3}' | grep -c "^1$" || echo "0")
TOTAL_SNAPSHOTS=15

echo "  Процесс $PID1 (CPU 0): $CORE0_COUNT/$TOTAL_SNAPSHOTS снимков на ядре 0"
echo "  Процесс $PID2 (CPU 1): $CORE1_COUNT/$TOTAL_SNAPSHOTS снимков на ядре 1"

echo ""
if [ $CORE0_COUNT -gt 10 ] && [ $CORE1_COUNT -gt 10 ]; then
    echo "CPU affinity работает КОРРЕКТНО"
    echo "Процессы закреплены на указанных ядрах"
    echo "Миграция между ядрами минимальна"
else
    echo "Процессы мигрируют между ядрами"
    echo "CPU affinity работает НЕПОЛНОСТЬЮ"
fi

echo ""
echo "  - $LOG_DIR/affinity_metrics.txt - метрики распределения"
echo "  - $LOG_DIR/affinity_cpu0.log   - лог процесса на CPU 0"
echo "  - $LOG_DIR/affinity_cpu1.log   - лог процесса на CPU 1"