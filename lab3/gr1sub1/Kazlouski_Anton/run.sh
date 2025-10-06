#!/bin/bash
# Демонстрация запуска утилиты pstat и сравнения с системными инструментами
PID=$1

if [ -z "$PID" ]; then
  echo "Usage: ./run.sh <pid>"
  exit 1
fi

echo "Running ./pstat $PID"
./src/pstat $PID

echo
echo "=== ps output ==="
ps -p $PID -o pid,ppid,thcount,state,time,rss,vsz,comm

echo
echo "=== vmmap summary ==="
vmmap -summary $PID | head -n 30

echo
echo "=== top snapshot ==="
top -pid $PID -stats pid,command,cpu,time,rsize | head -n 10
