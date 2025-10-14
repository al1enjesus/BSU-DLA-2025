#!/bin/bash

# Скрипт для непрерывного мониторинга процесса с помощью pstat

if [ $# -ne 1 ]; then
    echo "Usage: $0 <pid>"
    exit 1
fi

PID=$1
INTERVAL=2

echo "Мониторинг процесса PID $PID с интервалом $INTERVAL секунд"
echo "Нажмите Ctrl+C для остановки"
echo

while true; do
    echo "=== $(date) ==="
    ../bin/pstat $PID
    echo
    sleep $INTERVAL
done