#!/bin/bash

set -e

echo "=================================================="
echo "=== Task E*: Complete Diagnostics ==="
echo "=================================================="
echo ""

# Check if perf is available
if ! command -v perf &> /dev/null; then
    echo " perf is not installed. Please run:"
    echo "   sudo apt install linux-tools-$(uname -r) linux-tools-generic"
    exit 1
fi

echo " perf found: $(perf --version | head -1)"

cleanup() {
    echo "Cleaning up processes..."
    pkill -f "imitation-active" 2>/dev/null || true
    pkill -f "perf" 2>/dev/null || true
    rm -f test_file_*.bin 2>/dev/null || true
    rm -f strace_output.log 2>/dev/null || true
}

trap cleanup EXIT

# Build tools
echo " Building tools..."
make build-e
echo " Build complete"

echo ""
echo "=== 1. STRACE - SYSTEM CALL ANALYSIS ==="
echo "Running strace for 6 seconds..."
timeout 6 strace -c -f ./bin/imitation-active 2>&1 | tee strace_output.log | tail -20

echo ""
echo "=== 2. PERF STAT - HARDWARE COUNTERS ==="

# Запускаем программу в фоне
./bin/imitation-active > /dev/null 2>&1 &
PERF_PID=$!
sleep 1

echo "--- Method 1: Direct measurement ---"
perf stat -e cycles,instructions -p $PERF_PID --timeout 5000 2>&1 | head -15

echo ""
echo "--- Method 2: Alternative counters ---"  
perf stat -e context-switches,page-faults -p $PERF_PID --timeout 4000 2>&1 | head -15

# Завершаем процесс
kill $PERF_PID 2>/dev/null || true

echo ""
echo "=== 3. REAL-TIME PROCESS MONITORING ==="
echo "Starting process for live monitoring..."
./bin/imitation-active > /dev/null 2>&1 &
MONITOR_PID=$!
echo "Monitoring PID: $MONITOR_PID"

# Wait for process to start
sleep 2

if ps -p $MONITOR_PID > /dev/null 2>&1; then
    echo ""
    echo "--- pstat output ---"
    timeout 3 ./bin/pstat $MONITOR_PID 2>/dev/null | head -20 || echo "pstat completed"
    
    echo ""
    echo "--- system process info ---"
    ps -p $MONITOR_PID -o pid,ppid,comm,state,pcpu,pmem,rss,vsz,psr --no-headers 2>/dev/null || echo "Process ended"
    
    echo ""
    echo "--- resource usage ---"
    for i in {1..3}; do
        if ps -p $MONITOR_PID > /dev/null 2>&1; then
            echo "Second $i - CPU: $(ps -p $MONITOR_PID -o pcpu --no-headers 2>/dev/null || echo "0")% | MEM: $(ps -p $MONITOR_PID -o pmem --no-headers 2>/dev/null || echo "0")%"
            sleep 1
        else
            echo "Process ended during monitoring"
            break
        fi
    done
    
    # Cleanup
    kill $MONITOR_PID 2>/dev/null || true
else
    echo "Process terminated too quickly for monitoring"
fi

cleanup
