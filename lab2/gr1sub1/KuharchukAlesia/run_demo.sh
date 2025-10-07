#!/bin/bash

# Файлы для метрик
METRICS_FILE="demo_metrics.txt"
CPU_METRICS_FILE="cpu_metrics.txt"
MEMORY_METRICS_FILE="memory_metrics.txt"

echo "=== Starting demo with metrics collection ==="
echo "Demo started at: $(date)" > "$METRICS_FILE"

echo ""
echo "=== System information ==="
echo "=== System information ===" >> "$METRICS_FILE"
echo "CPU cores: $(nproc)" | tee -a "$METRICS_FILE"
echo "Kernel: $(uname -r)" | tee -a "$METRICS_FILE"
echo "Memory: $(grep MemTotal /proc/meminfo | awk '{print $2 $3}')" | tee -a "$METRICS_FILE"

echo ""
echo "=== Memory info before start ==="
echo "=== Memory info before start ===" >> "$METRICS_FILE"
grep -E "(MemTotal|MemFree|MemAvailable|SwapTotal|SwapFree)" /proc/meminfo >> "$METRICS_FILE"
grep -E "(MemTotal|MemFree|MemAvailable|SwapTotal|SwapFree)" /proc/meminfo | tee -a "$METRICS_FILE"

echo ""
echo "=== Checking if binaries exist ==="
if [ ! -f "src/supervisor" ] || [ ! -f "src/worker" ]; then
    echo "Binaries not found, compiling..."
    cd src
    gcc -Wall -Wextra -std=c99 -o supervisor supervisor.c config.c
    gcc -Wall -Wextra -std=c99 -o worker worker.c config.c
    cd ..
fi

echo ""
echo "=== Starting supervisor ==="
cd src
./supervisor &
SUPER_PID=$!

echo "Supervisor PID: $SUPER_PID"
sleep 2

# Получаем PID воркеров для мониторинга
WORKER_PIDS=$(ps --ppid $SUPER_PID -o pid= 2>/dev/null | tr '\n' ' ')
echo "Worker PIDs: $WORKER_PIDS"

echo ""
echo "=== Starting background metrics collection ==="
# Сбор метрик CPU для супервизора и воркеров
echo "=== CPU Metrics Collection ===" > "$CPU_METRICS_FILE"
if [ ! -z "$WORKER_PIDS" ]; then
    pidstat -u -p $SUPER_PID,$(echo $WORKER_PIDS | tr ' ' ',') 1 10 >> "$CPU_METRICS_FILE" 2>/dev/null &
else
    pidstat -u -p $SUPER_PID 1 10 >> "$CPU_METRICS_FILE" 2>/dev/null &
fi

# Сбор метрик памяти
echo "=== Memory Metrics Collection ===" > "$MEMORY_METRICS_FILE"
if [ ! -z "$WORKER_PIDS" ]; then
    pidstat -r -p $SUPER_PID,$(echo $WORKER_PIDS | tr ' ' ',') 1 10 >> "$MEMORY_METRICS_FILE" 2>/dev/null &
else
    pidstat -r -p $SUPER_PID 1 10 >> "$MEMORY_METRICS_FILE" 2>/dev/null &
fi

PIDSTAT_PID=$!

echo ""
echo "=== Demonstration ==="
echo ""

echo "1. Checking processes:"
ps aux | grep -E "(supervisor|worker)" | grep -v grep
echo ""

# Сохраняем информацию о планировании
echo "=== Process scheduling info ===" >> "$METRICS_FILE"
for pid in $SUPER_PID $WORKER_PIDS; do
    if [ -d "/proc/$pid" ] 2>/dev/null; then
        echo "PID $pid:" >> "$METRICS_FILE"
        echo "  Nice: $(ps -o ni= -p $pid 2>/dev/null)" >> "$METRICS_FILE"
        echo "  CPU: $(ps -o psr= -p $pid 2>/dev/null)" >> "$METRICS_FILE"
        echo "  State: $(ps -o stat= -p $pid 2>/dev/null)" >> "$METRICS_FILE"
    fi
done

sleep 2

echo ""
echo "2. Switching to LIGHT mode (SIGUSR1):"
kill -USR1 $SUPER_PID
sleep 3

echo "=== After LIGHT mode switch ===" >> "$METRICS_FILE"
ps -o pid,ni,psr,pcpu,pmem,comm -p $WORKER_PIDS 2>/dev/null >> "$METRICS_FILE"

echo ""
echo "3. Switching to HEAVY mode (SIGUSR2):"
kill -USR2 $SUPER_PID
sleep 3

echo "=== After HEAVY mode switch ===" >> "$METRICS_FILE"
ps -o pid,ni,psr,pcpu,pmem,comm -p $WORKER_PIDS 2>/dev/null >> "$METRICS_FILE"

echo ""
echo "4. Testing worker restart (simulating crash):"
# Находим первого воркера и "падаем" его
FIRST_WORKER=$(echo $WORKER_PIDS | awk '{print $1}')
if [ ! -z "$FIRST_WORKER" ]; then
    echo "Simulating crash of worker $FIRST_WORKER"
    kill -KILL $FIRST_WORKER
    sleep 2
    echo "=== After worker crash ===" >> "$METRICS_FILE"
    NEW_WORKER_PIDS=$(ps --ppid $SUPER_PID -o pid= 2>/dev/null | tr '\n' ' ')
    ps -o pid,ni,psr,pcpu,pmem,comm -p $NEW_WORKER_PIDS 2>/dev/null >> "$METRICS_FILE"
fi

echo ""
echo "5. Reloading config (SIGHUP):"
kill -HUP $SUPER_PID
sleep 2

echo ""
echo "6. Checking process states before shutdown:"
ps -o pid,ni,psr,pcpu,pmem,stat,comm -p $SUPER_PID,$(ps --ppid $SUPER_PID -o pid= 2>/dev/null) 2>/dev/null

echo ""
echo "7. Graceful shutdown (SIGTERM):"
kill -TERM $SUPER_PID
echo ""
echo "Waiting for shutdown..."
# Ждем завершения с таймаутом
for i in {1..10}; do
    if ! kill -0 $SUPER_PID 2>/dev/null; then
        break
    fi
    sleep 1
    echo "Waiting... $i/10"
done

# Если процесс еще жив, принудительно завершаем
if kill -0 $SUPER_PID 2>/dev/null; then
    echo "Forcing shutdown..."
    kill -KILL $SUPER_PID
fi

# Ждем завершения сбора метрик
echo "Waiting for metrics collection to finish..."
wait $PIDSTAT_PID 2>/dev/null

echo ""
echo "=== Final metrics summary ==="
echo "=== Final metrics summary ===" >> "$METRICS_FILE"

echo ""
echo "=== CPU Usage Statistics ===" | tee -a "$METRICS_FILE"
if [ -f "$CPU_METRICS_FILE" ]; then
    tail -20 "$CPU_METRICS_FILE" | tee -a "$METRICS_FILE"
else
    echo "CPU metrics file not found" | tee -a "$METRICS_FILE"
fi

echo ""
echo "=== Memory Usage Statistics ===" | tee -a "$METRICS_FILE"
if [ -f "$MEMORY_METRICS_FILE" ]; then
    tail -20 "$MEMORY_METRICS_FILE" | tee -a "$METRICS_FILE"
else
    echo "Memory metrics file not found" | tee -a "$METRICS_FILE"
fi

echo ""
echo "=== Memory info after demo ===" | tee -a "$METRICS_FILE"
grep -E "(MemTotal|MemFree|MemAvailable|SwapTotal|SwapFree)" /proc/meminfo | tee -a "$METRICS_FILE"

echo ""
echo "=== Process tree during demo ===" >> "$METRICS_FILE"
echo "Supervisor PID: $SUPER_PID" >> "$METRICS_FILE"
echo "Worker PIDs: $WORKER_PIDS" >> "$METRICS_FILE"

echo ""
echo "=== Demo completed ==="
echo "=== Detailed metrics saved to: ==="
echo "  - $METRICS_FILE (основные метрики)"
echo "  - $CPU_METRICS_FILE (метрики CPU)" 
echo "  - $MEMORY_METRICS_FILE (метрики памяти)"

# Анализ влияния nice
echo ""
echo "=== Nice priority analysis ===" | tee -a "$METRICS_FILE"
if [ -f "$CPU_METRICS_FILE" ]; then
    echo "CPU usage by process (showing nice priority effect):" | tee -a "$METRICS_FILE"
    grep -E "^Average|^[0-9].*worker" "$CPU_METRICS_FILE" | head -10 | tee -a "$METRICS_FILE"
fi

cd ..
