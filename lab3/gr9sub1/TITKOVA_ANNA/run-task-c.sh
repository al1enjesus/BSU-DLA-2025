#!/bin/bash

set -e

echo "=================================================="
echo "=== Task C: pstat utility and /proc metrics ==="
echo "=================================================="
echo ""

# Cleanup any previous processes
pkill -f "imitation" 2>/dev/null || true
rm -f pstat_io_test.bin 2>/dev/null || true

# Build tools for Task C
echo " Building tools for Task C..."
make build-c
echo ""

# Start imitation process
echo " Starting imitation process..."
./bin/imitation &
IMITATION_PID=$!
echo "Imitation process PID: $IMITATION_PID"
echo ""

# Wait for process to start and do initial work
echo " Waiting for process to complete file operations..."
sleep 3

echo " ============ pstat OUTPUT ============"
./bin/pstat $IMITATION_PID
echo ""

echo " ============ COMPARISON WITH SYSTEM TOOLS ============"
echo ""

echo "=== ps command ==="
ps -p $IMITATION_PID -o pid,ppid,comm,state,time,rss,vsz,psr --no-headers 2>/dev/null || echo "Process not found"
echo ""

echo "=== top command (snapshot) ==="
top -b -n1 -p $IMITATION_PID 2>/dev/null | head -15 || echo "top command failed"
echo ""

echo "=== pidstat command ==="
pidstat -p $IMITATION_PID 1 1 2>/dev/null || echo "pidstat not available"
echo ""

echo "=== /proc files (raw samples) ==="
if [ -d "/proc/$IMITATION_PID" ]; then
    echo "/proc/$IMITATION_PID/stat (first 200 chars):"
    cat /proc/$IMITATION_PID/stat 2>/dev/null | head -c 200 || echo "Cannot read stat"
    echo -e "\n..."

    echo "/proc/$IMITATION_PID/status (key fields):"
    grep -E "^(Pid|PPid|State|Threads|VmRSS|VmSize|voluntary|nonvoluntary)" /proc/$IMITATION_PID/status 2>/dev/null || echo "Cannot read status"

    echo "/proc/$IMITATION_PID/io:"
    cat /proc/$IMITATION_PID/io 2>/dev/null || echo "Cannot read io"
else
    echo "Process directory /proc/$IMITATION_PID not found - process may have terminated"
fi
echo ""

# Wait for process to complete
echo " Waiting for imitation process to complete..."
if wait $IMITATION_PID 2>/dev/null; then
    echo "Process completed successfully"
else
    echo "Process already completed"
fi

