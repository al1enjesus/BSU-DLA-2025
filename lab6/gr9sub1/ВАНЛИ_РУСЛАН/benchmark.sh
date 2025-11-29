#!/bin/bash

# Настройки
FUSE_DIR="/tmp/fuse"
NATIVE_DIR="/tmp/source"
RESULT_FILE="results.csv"

# Проверка, смонтировано ли
if ! mount | grep -q "$FUSE_DIR"; then
    echo "Error: FUSE is not mounted at $FUSE_DIR"
    exit 1
fi

# Инициализация CSV
echo "TestType,Operation,FileSystem,Value,Unit" > $RESULT_FILE

# Функция для измерения времени выполнения команды
measure_time() {
    local start=$(date +%s%N)
    "$@" > /dev/null 2>&1
    local end=$(date +%s%N)
    # Возвращаем время в секундах с плавающей точкой
    echo | awk "{print ($end - $start) / 1000000000}"
}

run_benchmark() {
    DIR=$1
    FS_NAME=$2
    
    echo "------------------------------------------------"
    echo "Running benchmarks on: $FS_NAME ($DIR)"

    # --- 1. LATENCY TEST (Задержка) ---
    echo "  [1/3] Testing Latency (1000 operations)..."
    
    # Create/Open Latency
    TIME=$(measure_time bash -c "for i in {1..1000}; do touch $DIR/test_lat_\$i; done")
    # Latency в мс = (Total Time / 1000) * 1000 = Total Time
    LATENCY=$(echo "$TIME" | awk '{print $1 * 1000 / 1000}') 
    echo "Latency,Create,$FS_NAME,$LATENCY,ms" >> $RESULT_FILE

    # Metadata (Getattr) Latency
    TIME=$(measure_time bash -c "for i in {1..1000}; do stat $DIR/test_lat_\$i; done")
    LATENCY=$(echo "$TIME" | awk '{print $1 * 1000 / 1000}')
    echo "Latency,Getattr,$FS_NAME,$LATENCY,ms" >> $RESULT_FILE

    # Unlink Latency (cleanup)
    TIME=$(measure_time bash -c "for i in {1..1000}; do rm $DIR/test_lat_\$i; done")
    LATENCY=$(echo "$TIME" | awk '{print $1 * 1000 / 1000}')
    echo "Latency,Unlink,$FS_NAME,$LATENCY,ms" >> $RESULT_FILE

    # --- 2. THROUGHPUT TEST (Пропускная способность) ---
    echo "  [2/3] Testing Throughput (100MB file)..."
    
    # Write Speed
    # sync перед тестом, чтобы сбросить кеши (насколько возможно без sudo)
    sync
    TIME=$(measure_time dd if=/dev/zero of=$DIR/bigfile bs=1M count=100 conv=fsync)
    # Speed in MB/s = 100 / Time
    SPEED=$(echo "$TIME" | awk '{print 100 / $1}')
    echo "Throughput,Write,$FS_NAME,$SPEED,MB/s" >> $RESULT_FILE

    # Read Speed
    sync
    # Чистим кеш страниц в Linux (требует sudo, если нет прав - просто sync)
    # sudo sh -c "echo 3 > /proc/sys/vm/drop_caches" 2>/dev/null
    TIME=$(measure_time dd if=$DIR/bigfile of=/dev/null bs=1M count=100)
    SPEED=$(echo "$TIME" | awk '{print 100 / $1}')
    echo "Throughput,Read,$FS_NAME,$SPEED,MB/s" >> $RESULT_FILE

    rm $DIR/bigfile

    # --- 3. IOPS TEST (Операций в секунду) ---
    echo "  [3/3] Testing IOPS (Small files creation)..."
    
    # Создаем 1000 файлов по 1КБ
    TIME=$(measure_time bash -c "for i in {1..1000}; do dd if=/dev/zero of=$DIR/small_\$i bs=1024 count=1; done")
    # IOPS = 1000 / Time
    IOPS=$(echo "$TIME" | awk '{print 1000 / $1}')
    echo "IOPS,Create_1K,$FS_NAME,$IOPS,ops/sec" >> $RESULT_FILE
    
    # Cleanup
    rm $DIR/small_* 2>/dev/null
}

# Запуск тестов
run_benchmark "$NATIVE_DIR" "Native(ext4/tmp)"
run_benchmark "$FUSE_DIR" "FUSE"

echo "------------------------------------------------"
echo "Benchmark finished. Results saved to $RESULT_FILE"
