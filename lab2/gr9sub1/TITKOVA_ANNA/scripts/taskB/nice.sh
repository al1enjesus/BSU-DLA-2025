#!/bin/bash
# Демонстрация влияния nice с анализом результатов

echo "=================================================="
echo "              NICE ДЕМОНСТРАЦИЯ"
echo "=================================================="

# Все пути относительно корня проекта
BIN_DIR="./bin"
LOG_DIR="./logs"
mkdir -p "$LOG_DIR"

echo "Текущая директория: $(pwd)"
echo "Bin dir: $BIN_DIR"
echo "Log dir: $LOG_DIR"

cleanup() {
    pkill -f "./cpu_burn" 2>/dev/null || true
    pkill -f "cpu_burn" 2>/dev/null || true
    sleep 1
}

trap cleanup EXIT INT TERM

# Проверяем что cpu_burn существует
if [ ! -f "$BIN_DIR/cpu_burn" ]; then
    echo "ОШИБКА: $BIN_DIR/cpu_burn не найден!"
    echo "Сначала выполните сборку: make build"
    exit 1
fi

echo "Запускаем 2 процесса на CPU 0 с разными nice..."
echo ""

# Запускаем процессы из корневой директории
echo "Запуск процесса с nice=0..."
"$BIN_DIR/cpu_burn" --work-us 8000 --sleep-us 2000 --duration 20 --cpu 0 > "$LOG_DIR/nice0.log" 2>&1 &
PID1=$!
echo "Процесс 1: PID $PID1, nice=0 (высокий приоритет)"

echo "Запуск процесса с nice=10..."
nice -n 10 "$BIN_DIR/cpu_burn" --work-us 8000 --sleep-us 2000 --duration 20 --cpu 0 > "$LOG_DIR/nice10.log" 2>&1 &
PID2=$!
echo "Процесс 2: PID $PID2, nice=10 (низкий приоритет)"

echo ""
echo "=== СБОР МЕТРИК С PIDSTAT ==="
echo "Собираем метрики в течение 15 секунд..."
pidstat -p $PID1,$PID2 1 15 > "$LOG_DIR/pidstat_nice.txt" 2>&1 &
PIDSTAT_PID=$!

echo ""
echo "=== МОНИТОРИНГ В РЕАЛЬНОМ ВРЕМЕНИ ==="
echo "PID     NI CPU %CPU COMMAND"
for i in {1..15}; do
    echo "--- Снимок $i/15 ---"
    ps -p $PID1,$PID2 -o pid,ni,psr,pcpu,comm 2>/dev/null | tail -2 || echo "Процессы завершились"
    sleep 1
done

echo ""
echo "Завершаем сбор метрик..."
wait $PIDSTAT_PID 2>/dev/null

echo ""
echo "Ожидаем завершения процессов..."
wait $PID1 $PID2 2>/dev/null || echo "Процессы завершены"

echo ""
echo "=== АНАЛИЗ РЕЗУЛЬТАТОВ ==="
echo ""

# Проверяем что файлы создались
echo "Проверка созданных файлов в $LOG_DIR/:"
ls -la "$LOG_DIR/" | grep -E "(nice|pidstat)"

# Анализ pidstat
echo ""
echo "СРЕДНИЕ ЗНАЧЕНИЯ ИЗ PIDSTAT:"
if [ -f "$LOG_DIR/pidstat_nice.txt" ]; then
    grep -A2 "Среднее:" "$LOG_DIR/pidstat_nice.txt" | tail -3
else
    echo "Файл с метриками не найден: $LOG_DIR/pidstat_nice.txt"
fi

echo ""
echo "РАСПРЕДЕЛЕНИЕ CPU:"
if [ -f "$LOG_DIR/pidstat_nice.txt" ]; then
    CPU_NICE0=$(grep " $PID1 " "$LOG_DIR/pidstat_nice.txt" 2>/dev/null | awk '{sum+=$8; count++} END {if(count>0) printf "%.1f", sum/count; else print "N/A"}')
    CPU_NICE10=$(grep " $PID2 " "$LOG_DIR/pidstat_nice.txt" 2>/dev/null | awk '{sum+=$8; count++} END {if(count>0) printf "%.1f", sum/count; else print "N/A"}')

    echo "  Nice=0:  ${CPU_NICE0}% CPU"
    echo "  Nice=10: ${CPU_NICE10}% CPU"

    echo ""
    if [ "$CPU_NICE0" != "N/A" ] && [ "$CPU_NICE10" != "N/A" ]; then
        if (( $(echo "$CPU_NICE0 > $CPU_NICE10" | bc -l 2>/dev/null || echo "1") )); then
            echo " Процесс с nice=0 получает БОЛЬШЕ CPU времени"
            echo " Nice значения работают корректно"
        else
            echo " Разница в распределении CPU незначительна"
        fi
    fi
else
    echo "Файл с метриками не найден для анализа"
fi

echo ""
echo "ЛОГИ ПРОЦЕССОВ:"
if [ -f "$LOG_DIR/nice0.log" ]; then
    echo "  nice0.log: $(wc -l < "$LOG_DIR/nice0.log") строк"
else
    echo "  nice0.log: не найден"
fi

if [ -f "$LOG_DIR/nice10.log" ]; then
    echo "  nice10.log: $(wc -l < "$LOG_DIR/nice10.log") строк"
else
    echo "  nice10.log: не найден"
fi

echo ""
echo "=== ЗАВЕРШЕНО ==="
