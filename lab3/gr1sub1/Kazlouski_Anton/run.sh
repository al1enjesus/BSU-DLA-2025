#!/bin/bash
PID=$1
if [ -z "$PID" ]; then
  echo "Usage: $0 <pid>"
  exit 1
fi

./src/pstat $PID
echo
ps -p $PID -o pid,ppid,state,time,rss,vsz,comm
echo
top -p $PID -b -n 1 | head -n 20
echo
cat /proc/$PID/status | grep -E 'Threads|voluntary_ctxt_switches|nonvoluntary_ctxt_switches'
echo
cat /proc/$PID/io
