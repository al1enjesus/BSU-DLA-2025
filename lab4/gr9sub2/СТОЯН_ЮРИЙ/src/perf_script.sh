#!/bin/bash

echo "=== PERF STAT ANALYSIS SCRIPT ==="

# Компилируем бенчмарки
echo "1. Компиляция бенчмарков..."
gcc -O2 -o benchmark_open benchmark_open.c
gcc -O2 -o benchmark_heavy benchmark_heavy.c

echo -e "\n2. Запуск perf stat для benchmark_open..."
echo "=== PERF STAT FOR benchmark_open ===" > perf_analysis.txt
perf stat -e cycles,instructions,context-switches,page-faults ./benchmark_open 2>> perf_analysis.txt

echo -e "\n3. Запуск perf stat для benchmark_heavy..."
echo -e "\n=== PERF STAT FOR benchmark_heavy ===" >> perf_analysis.txt
perf stat -e cycles,instructions,context-switches,page-faults ./benchmark_heavy 2>> perf_analysis.txt

echo -e "\n4. Дополнительные метрики..."
echo -e "\n=== EXTENDED METRICS ===" >> perf_analysis.txt
perf stat -e cycles,instructions,cache-misses,cache-references,branch-misses,branches ./benchmark_open 2>> perf_analysis.txt

echo -e "\n5. Результаты сохранены в perf_analysis.txt"
echo "=== KEY METRICS SUMMARY ==="
grep -E "cycles|instructions|context-switches|page-faults|seconds" perf_analysis.txt | head -20