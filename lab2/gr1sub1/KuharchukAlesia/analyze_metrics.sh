#!/bin/bash

echo "=== Metrics Analysis ==="
echo ""

if [ ! -f "demo_metrics.txt" ]; then
    echo "Error: demo_metrics.txt not found. Run run_demo.sh first."
    exit 1
fi

echo "1. System Information:"
grep -E "(CPU cores|Kernel|Memory:)" demo_metrics.txt | head -5

echo ""
echo "2. Memory Usage Before/After:"
echo "Before demo:"
grep -A5 "Memory info before start" demo_metrics.txt | grep -E "(MemTotal|MemFree|MemAvailable)" | head -3
echo ""
echo "After demo:"
grep -A5 "Memory info after demo" demo_metrics.txt | grep -E "(MemTotal|MemFree|MemAvailable)" | head -3

echo ""
echo "3. Process Scheduling Information:"
grep -A10 "Process scheduling info" demo_metrics.txt | head -15

echo ""
echo "4. Nice Priority Effect Analysis:"
echo "Based on process states:"
grep -E "Worker.*PID.*nice" demo_metrics.txt | head -5

if [ -f "cpu_metrics.txt" ]; then
    echo ""
    echo "5. CPU Usage from pidstat:"
    grep -E "Average|worker" cpu_metrics.txt | head -10
else
    echo ""
    echo "5. CPU metrics file not available"
fi

echo ""
echo "=== Analysis Complete ==="
