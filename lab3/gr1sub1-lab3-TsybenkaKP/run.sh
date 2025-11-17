#!/bin/bash

LOG_DIR="./src"
mkdir -p "$LOG_DIR"


DEMO_LOG="$LOG_DIR/demo.log"
STRACE_LOG="$LOG_DIR/strace.log"
PERF_LOG="$LOG_DIR/perf.log"


> "$DEMO_LOG"
> "$STRACE_LOG"
> "$PERF_LOG"

echo "[*] Запуск тестового процесса (CPU burn)" | tee -a "$DEMO_LOG"

yes > /dev/null &
CPU_BURN_PID=$!
echo "PID=$CPU_BURN_PID" | tee -a "$DEMO_LOG"

sleep 1

echo -e "\n=== pstat.py ===" | tee -a "$DEMO_LOG"

if [[ -x "./src/pstat.py" ]]; then
    ./src/pstat.py "$CPU_BURN_PID" | tee -a "$DEMO_LOG"
else
    echo "Ошибка: ./src/pstat.py не найден или не исполняемый" | tee -a "$DEMO_LOG"
fi

echo -e "\n=== ps ===" | tee -a "$DEMO_LOG"
ps -p "$CPU_BURN_PID" -o pid,ppid,state,nlwp,rss,comm | tee -a "$DEMO_LOG"

echo -e "\n=== pidstat ===" | tee -a "$DEMO_LOG"
pidstat -p "$CPU_BURN_PID" 1 1 | tee -a "$DEMO_LOG"

echo -e "\n=== top (1 итерация) ===" | tee -a "$DEMO_LOG"
top -b -n 1 -p "$CPU_BURN_PID" | tee -a "$DEMO_LOG"

echo -e "\n=== strace -c (5 секунд) ==="
timeout 5 strace -c -p "$CPU_BURN_PID" &> "$STRACE_LOG"
if [[ $? -eq 124 ]]; then
    echo "[strace] Завершено по таймауту 5 секунд" >> "$STRACE_LOG"
fi

echo -e "\n=== perf stat (5 секунд) ==="
if [[ $EUID -ne 0 ]]; then
    echo "perf stat требует root. Пропускаем." | tee -a "$PERF_LOG"
else
    timeout 5 perf stat -p "$CPU_BURN_PID" sleep 5 &> "$PERF_LOG"
    if [[ $? -eq 124 ]]; then
        echo "[perf] Завершено по таймауту 5 секунд" >> "$PERF_LOG"
    fi
fi

kill "$CPU_BURN_PID" &> /dev/null
wait "$CPU_BURN_PID" 2>/dev/null

echo "[*] Процесс $CPU_BURN_PID завершён"

