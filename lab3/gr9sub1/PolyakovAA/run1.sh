#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <PID>"
    exit 1
fi

PID=$1
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")

# Создаем папку logs если её нет
LOG_DIR="logs"
mkdir -p "$LOG_DIR"

# Явно указываем путь к perf
PERF_CMD="/usr/lib/linux-tools-6.8.0-85/perf"

# Запускаем команды и сохраняем результаты в папку logs
ps -p $PID -o pid,ppid,state,time,thcount,rss > "$LOG_DIR/ps_result.txt" &
top -b -n 1 -p $PID > "$LOG_DIR/top_result.txt" &
pidstat -p $PID 1 5 > "$LOG_DIR/pidstat_result.txt" &
python3 src/pstat.py $PID > "$LOG_DIR/pstat_resulttxt" &
timeout 5 strace -f -c -p $PID 2> "$LOG_DIR/strace_result.txt" &
sudo $PERF_CMD stat -p $PID sleep 5 > "$LOG_DIR/perf_result.txt" 2>&1 &

# Ждем завершения всех фоновых процессов
wait
