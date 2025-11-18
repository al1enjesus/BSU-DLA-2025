mkdir -p logs

# Измерение основных метрик производительности
perf stat -e cycles,instructions,cache-misses,cache-references,branches,branch-misses,page-faults,context-switches,cpu-migrations,L1-dcache-loads,L1-dcache-load-misses,L1-icache-load-misses ./build/benchmark 2> logs/perf_stat_detailed.log

# Анализ распределения времени выполнения
perf record -g -o logs/perf.data ./build/benchmark
perf report -i logs/perf.data --stdio > logs/perf_report.log