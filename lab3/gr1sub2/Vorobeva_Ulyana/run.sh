#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$SCRIPT_DIR/src/pstat.py"
if [ ! -x "$SRC" ]; then
chmod +x "$SRC" || true
fi
if [ $# -eq 0 ]; then
PID=$$
else
PID=$1
fi
echo "Running pstat for PID $PID"
python3 "$SRC" "$PID"


# show quick comparisons
echo -e "\n--- ps output ---"
ps -p "$PID" -o pid,ppid,state,utime,stime,rss,nlwp,comm 2>/dev/null || true


echo -e "\n--- top (one snapshot) ---"
top -b -n 1 -p "$PID" 2>/dev/null | sed -n '1,20p'


if command -v pidstat >/dev/null 2>&1; then
echo -e "\n--- pidstat (1s sample) ---"
pidstat -p "$PID" 1 1
fi


if command -v strace >/dev/null 2>&1; then
echo -e "\n--- strace -c (short 2s attach; may require root) ---"
if [ "$PID" -ne "$$" ]; then
sudo strace -f -c -p "$PID" -o /tmp/strace_pstat.out -q -s 64 &
sleep 2
sudo kill -INT $(pgrep -f "strace -f -c -p $PID") 2>/dev/null || true
echo "strace summary (if produced):"
sudo cat /tmp/strace_pstat.out || true
else
echo "Skipping strace on current shell to avoid noise"
fi
fi


if command -v perf >/dev/null 2>&1; then
echo -e "\n--- perf stat (5s) ---"
sudo perf stat -p "$PID" sleep 1 2>/dev/null || true
fi