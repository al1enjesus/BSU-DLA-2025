#!/bin/bash

# Пути
SRC="/tmp/source"
MNT="/tmp/mount"
CSV="results.csv"

# Функция замера
measure() {
    fs=$1       # Native или FUSE
    category=$2 # Latency, Throughput, IOPS
    param=$3    # open/read... или размер (10) или кол-во (1000)
    cmd=$4      # Команда

    TIMEFORMAT=%R
    duration=$({ time bash -c "$cmd" > /dev/null 2>&1; } 2>&1)
    
    echo "$fs,$category,$param,$duration" >> $CSV
    
    printf "%-8s | %-12s | %-10s | %s sec\n" "$fs" "$category" "$param" "$duration"
}

run_suite() {
    NAME=$1
    DIR=$2
    
    echo "=== Тестирование: $NAME ($DIR) ==="

    # --- 1. LATENCY (Задержка операции) ---
    # open (создание файла)
    measure "$NAME" "Latency" "open" "touch $DIR/lat_open"
    # write (4KB)
    measure "$NAME" "Latency" "write" "dd if=/dev/zero of=$DIR/lat_write bs=4k count=1 oflag=sync"
    # read (4KB)
    measure "$NAME" "Latency" "read" "dd if=$DIR/lat_write of=/dev/null bs=4k count=1"
    # getattr (stat)
    measure "$NAME" "Latency" "getattr" "stat $DIR/lat_open"
    
    # Очистка
    rm -f $DIR/lat_*

    # --- 2. THROUGHPUT (Скорость записи) ---
    for size in 1 10 100; do
        measure "$NAME" "Throughput" "$size" "dd if=/dev/zero of=$DIR/th_${size} bs=1M count=$size oflag=sync"
        rm -f "$DIR/th_${size}"
    done

    # --- 3. IOPS (Создание множества файлов) ---
    for count in 1000; do
        measure "$NAME" "IOPS" "$count" "for i in {1..$count}; do touch $DIR/iops_\$i; done"
        rm -f $DIR/iops_*
    done
    echo ""
}

# Инициализация CSV
echo "Filesystem,Category,Parameter,Value" > $CSV

# Проверка
if [ ! -d "$MNT" ]; then echo "Error: $MNT not found"; exit 1; fi

run_suite "Native" "$SRC"
run_suite "FUSE" "$MNT"

echo "Готово. Данные в $CSV"
