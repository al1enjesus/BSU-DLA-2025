#!/bin/bash
set -e
mkdir -p logs

# 1) Сборка
make all

# 2) Задание A: LD_PRELOAD на выбранных программах (ls, cat, grep)
# Перенаправляем stderr (куда spy пишет) в logs/...

echo "=== LD_PRELOAD experiments (logs will be in logs/) ==="

# ls (пример: /etc/apt)
echo "Running: LD_PRELOAD=./libsyscall_spy.so ls -la /etc/apt" > logs/ls.out
LD_PRELOAD=./libsyscall_spy.so ls -la /etc/apt >> logs/ls.out 2>&1 || true

# cat (пример: /etc/hosts)
echo "Running: LD_PRELOAD=./libsyscall_spy.so cat /etc/hosts" > logs/cat.out
LD_PRELOAD=./libsyscall_spy.so cat /etc/hosts >> logs/cat.out 2>&1 || true

# grep (пример: поиск в /etc/apt)
echo "Running: LD_PRELOAD=./libsyscall_spy.so grep -R '^#' /etc/apt | head -n 1000" > logs/grep.out
{ LD_PRELOAD=./libsyscall_spy.so grep -R '^#' /etc/apt 2>&1 || true; } | head -n 1000 >> logs/grep.out 2>&1

# 3) Эксперимент: статическая программа
echo "Running static hello (should NOT be intercepted)" > logs/hello_static.out
LD_PRELOAD=./libsyscall_spy.so ./hello_static >> logs/hello_static.out 2>&1 || true

# 4) Задание B: Benchmark
echo "Running benchmark..." > logs/benchmark.out
./benchmark 2>> logs/benchmark.out >> logs/benchmark.out || true

# 5) Perf stat (требует sudo возможно)
echo "Running perf stat (may require sudo)" > logs/perf_stat.out
if command -v perf >/dev/null 2>&1; then
  sudo perf stat -e cycles,instructions,context-switches,page-faults -o logs/perf_stat.out ./benchmark || true
else
  echo "perf not found" >> logs/perf_stat.out
fi

# 6) Experiment with drop_caches: cold vs warm open
# Запустим отдельную бенч-программу для open only (с небольшим числом итераций)
# Cold cache
sudo sync; echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
./open_bench > logs/open_bench_cold.out 2>&1 || true
# Warm cache
./open_bench > logs/open_bench_warm.out 2>&1 || true

echo "All experiments done. Logs are in ./logs/"