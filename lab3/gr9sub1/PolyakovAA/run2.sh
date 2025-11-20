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

# Запускаем команды и сохраняем результаты в папку logs
ps -p $PID -o pid,ppid,state,time,thcount,rss > "$LOG_DIR/ps_result.txt" &
top -b -n 1 -p $PID > "$LOG_DIR/top_result.txt" &
pidstat -p $PID 1 5 > "$LOG_DIR/pidstat_result.txt" &
python3 src/pstat.py $PID > "$LOG_DIR/pstat_result.txt" &
timeout 5 strace -f -c -p $PID 2> "$LOG_DIR/strace_result.txt" &
perf stat -p $PID sleep 5 2> "$LOG_DIR/perf_result.txt" &

# Ждем завершения всех фоновых процессов
wait
