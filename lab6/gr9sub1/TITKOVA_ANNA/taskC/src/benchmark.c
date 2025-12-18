#!/bin/bash
# benchmark.sh - Нагрузочное тестирование Monitoring FUSE

set -e

echo "=== Monitoring FUSE Load Testing ==="

# Создание тестовых директорий
mkdir -p /tmp/bench_source /tmp/bench_mount

# Компиляция (если нужно)
make clean
make

# Функция для измерения времени
measure_time() {
    local desc="$1"
    shift
    echo -n "$desc: "
    /usr/bin/time -f "%e seconds" "$@" 2>&1 | tail -1
}

# Запуск файловой системы
echo "Starting filesystem..."
./monitoring_fuse /tmp/bench_source /tmp/bench_mount -f &
FS_PID=$!
sleep 2

echo -e "\n--- Test 1: Small file operations (IOPS) ---"
measure_time "Create 1000 files" bash -c "for i in {1..1000}; do echo 'test' > /tmp/bench_mount/file_\$i.txt; done"
measure_time "Read 1000 files" bash -c "for i in {1..1000}; do cat /tmp/bench_mount/file_\$i.txt > /dev/null; done"
measure_time "Delete 1000 files" bash -c "for i in {1..1000}; do rm /tmp/bench_mount/file_\$i.txt; done"

echo -e "\n--- Test 2: Sequential read/write ---"
echo "Creating 100MB test file..."
dd if=/dev/urandom of=/tmp/bench_mount/largefile bs=1M count=100 2>/dev/null

measure_time "Sequential read 100MB" dd if=/tmp/bench_mount/largefile of=/dev/null bs=1M 2>/dev/null
measure_time "Sequential write 50MB" dd if=/dev/zero of=/tmp/bench_mount/write_test bs=1M count=50 2>/dev/null

echo -e "\n--- Test 3: Mixed workload ---"
measure_time "Mixed operations" bash -c "
    for i in {1..500}; do
        echo 'data' > /tmp/bench_mount/mixed_\$i.dat
        cat /tmp/bench_mount/mixed_\$i.dat > /dev/null
    done
"

echo -e "\n--- Test 4: Metadata operations ---"
measure_time "Directory listing" ls -la /tmp/bench_mount/ > /dev/null
measure_time "Stat operations" bash -c "for i in {1..1000}; do stat /tmp/bench_mount/largefile > /dev/null; done"

echo -e "\n--- Test 5: Virtual file access ---"
echo "Reading statistics:"
time for i in {1..100}; do
    cat /tmp/bench_mount/.stats > /dev/null
done

echo -e "\n=== Final Statistics ==="
cat /tmp/bench_mount/.stats

# Очистка
echo -e "\nCleaning up..."
fusermount -u /tmp/bench_mount 2>/dev/null || kill $FS_PID 2>/dev/null || true
wait $FS_PID 2>/dev/null || true

rm -rf /tmp/bench_source /tmp/bench_mount

echo "Benchmark completed!"
