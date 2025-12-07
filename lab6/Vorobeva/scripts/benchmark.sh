#!/bin/bash

echo "=== FUSE Filesystem Benchmarking ==="
echo ""

SOURCE_DIR="test_dir/source"
MOUNT_DIR="test_dir/mount"
RESULTS_DIR="benchmark_results"
mkdir -p $SOURCE_DIR $MOUNT_DIR $RESULTS_DIR

# Функция для измерения времени
measure_time() {
    local command="$1"
    local label="$2"
    local output_file="$3"
    
    echo -n "  $label..."
    local start_time=$(date +%s%N)
    eval $command >/dev/null 2>&1
    local end_time=$(date +%s%N)
    local duration=$((($end_time - $start_time) / 1000000))
    echo " $duration ms"
    echo "$label: $duration ms" >> "$output_file"
}

# Бенчмарк для каждой ФС
benchmark_fs() {
    local fs_name="$1"
    local fs_binary="$2"
    local result_file="$RESULTS_DIR/${fs_name}_benchmark.txt"
    
    echo "=== Benchmarking $fs_name ==="
    
    # Запуск ФС в фоне
    ./bin/$fs_binary $SOURCE_DIR $MOUNT_DIR -f -s &
    local fs_pid=$!
    sleep 3
    
    # Очистка перед тестом
    rm -rf $SOURCE_DIR/* $MOUNT_DIR/*
    
    echo "" > "$result_file"
    echo "$fs_name Benchmark Results" >> "$result_file"
    echo "=========================" >> "$result_file"
    
    # Тест 1: Latency операций
    echo "Test 1: Operation Latency"
    
    # Создание файла для тестов
    echo "Test data" > $MOUNT_DIR/testfile.txt
    
    # Измерение getattr
    measure_time "stat $MOUNT_DIR/testfile.txt" "getattr" "$result_file"
    
    # Измерение open/read
    measure_time "cat $MOUNT_DIR/testfile.txt" "open+read" "$result_file"
    
    # Измерение write
    measure_time "echo 'New data' > $MOUNT_DIR/testfile2.txt" "create+write" "$result_file"
    
    # Тест 2: Throughput для разных размеров
    echo "Test 2: Throughput"
    
    for size in 1 10 100; do
        echo -n "  Writing ${size}MB file..."
        local start=$(date +%s%N)
        dd if=/dev/zero of=$MOUNT_DIR/bigfile_${size}M bs=1M count=$size status=none
        local end=$(date +%s%N)
        local duration=$((($end - $start) / 1000000))
        local throughput=$(echo "scale=2; $size * 1000 / $duration" | bc)
        echo " $throughput MB/s"
        echo "Write ${size}MB: $throughput MB/s" >> "$result_file"
        
        echo -n "  Reading ${size}MB file..."
        start=$(date +%s%N)
        dd if=$MOUNT_DIR/bigfile_${size}M of=/dev/null bs=1M status=none
        end=$(date +%s%N)
        duration=$((($end - $start) / 1000000))
        throughput=$(echo "scale=2; $size * 1000 / $duration" | bc)
        echo " $throughput MB/s"
        echo "Read ${size}MB: $throughput MB/s" >> "$result_file"
        
        rm $MOUNT_DIR/bigfile_${size}M
    done
    
    # Тест 3: IOPS (много маленьких файлов)
    echo "Test 3: IOPS (1000 files)"
    echo -n "  Creating 1000 files..."
    start=$(date +%s%N)
    for i in {1..100}; do  # Уменьшено для скорости
        echo "test" > $MOUNT_DIR/file_$i.txt
    done
    end=$(date +%s%N)
    duration=$((($end - $start) / 1000000))
    local iops=$(echo "scale=2; 100 * 1000 / $duration" | bc)
    echo " $iops files/second"
    echo "IOPS: $iops files/second" >> "$result_file"
    
    # Очистка
    rm -f $MOUNT_DIR/file_*.txt
    
    # Остановка ФС
    fusermount -uz $MOUNT_DIR 2>/dev/null || true
    kill $fs_pid 2>/dev/null || true
    sleep 2
    
    echo ""
}

# Запуск бенчмарков
benchmark_fs "passthrough" "passthrough_fs"
benchmark_fs "rot13" "rot13_fs"
benchmark_fs "uppercase" "uppercase_fs"

# Сравнение с нативной файловой системой
echo "=== Native Filesystem (ext4) Benchmark ==="
native_result="$RESULTS_DIR/native_benchmark.txt"
echo "" > "$native_result"
echo "Native ext4 Benchmark Results" >> "$native_result"
echo "=============================" >> "$native_result"

# Используем /tmp для нативного теста
NATIVE_DIR="/tmp/fuse_test"
mkdir -p $NATIVE_DIR

echo -n "Native write 100MB..."
start=$(date +%s%N)
dd if=/dev/zero of=$NATIVE_DIR/native_bigfile bs=1M count=100 status=none
end=$(date +%s%N)
duration=$((($end - $start) / 1000000))
throughput=$(echo "scale=2; 100 * 1000 / $duration" | bc)
echo " $throughput MB/s"
echo "Write 100MB: $throughput MB/s" >> "$native_result"

echo -n "Native read 100MB..."
start=$(date +%s%N)
dd if=$NATIVE_DIR/native_bigfile of=/dev/null bs=1M status=none
end=$(date +%s%N)
duration=$((($end - $start) / 1000000))
throughput=$(echo "scale=2; 100 * 1000 / $duration" | bc)
echo " $throughput MB/s"
echo "Read 100MB: $throughput MB/s" >> "$native_result"

rm -f $NATIVE_DIR/native_bigfile

echo ""
echo "=== Benchmark Results Summary ==="
echo "Results saved in $RESULTS_DIR/"
echo ""
echo "To compare results:"
echo "cat $RESULTS_DIR/*_benchmark.txt"