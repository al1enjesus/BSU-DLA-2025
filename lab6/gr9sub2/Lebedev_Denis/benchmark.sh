#!/bin/bash

# Скрипт для нагрузочного тестирования ФС
# Использование: ./benchmark.sh <test_directory> <fs_type>
# fs_type: "native" или "fuse"

set -e

if [ "$#" -ne 2 ]; then
    echo "Использование: $0 <test_directory> <fs_type>"
    exit 1
fi

TEST_DIR=$1
FS_TYPE=$2

SCRIPT_DIR=$(pwd)
RESULTS_FILE="$SCRIPT_DIR/results.csv"

# ===== Функция для извлечения скорости из dd =====
# Принимает строку вида "123 MB/s" или "1.2 GB/s"
# Возвращает число в MB/s
parse_speed() {
    local raw="$1"

    # Извлекаем "<число> <единица>/s"
    local value=$(echo "$raw" | grep -o '[0-9.,]\+ [kMG]B/s' | awk '{print $1}' | sed 's/,/./')
    local unit=$(echo "$raw" | grep -o '[0-9.,]\+ [kMG]B/s' | awk '{print $2}')

    # Если не найдено — вернуть пусто
    if [ -z "$value" ]; then
        echo ""
        return
    fi

    case "$unit" in
        "kB/s") echo "$(echo "$value / 1024" | bc -l)" ;;
        "MB/s") echo "$value" ;;
        "GB/s") echo "$(echo "$value * 1024" | bc -l)" ;;
        *) echo "" ;;
    esac
}

# ==== Создаём CSV, если нет ====
if [ ! -f "$RESULTS_FILE" ]; then
    echo "fs_type,test_type,value_mb_s,file_size_mb" > "$RESULTS_FILE"
fi

echo "--- Тестирование директории: $TEST_DIR (Тип: $FS_TYPE) ---"

# Подготовка
echo "Подготовка..."
rm -rf "$TEST_DIR"/*
mkdir -p "$TEST_DIR"
cd "$TEST_DIR"


# ===== Тест 1: Throughput =====
echo "Тест 1: Пропускная способность (dd)..."

for size_mb in 1 10 100; do
    echo "  - Размер файла: ${size_mb}MB"

    # ---- Write ----
    raw_write=$( (dd if=/dev/zero of="file_${size_mb}M" bs=1M count=${size_mb} conv=fsync) 2>&1 )
    write_speed=$(parse_speed "$raw_write")

    echo "$FS_TYPE,write,$write_speed,$size_mb" >> "$RESULTS_FILE"
    echo "    Скорость записи: ${write_speed} MB/s"

    # ---- Read ----
    raw_read=$( (dd if="file_${size_mb}M" of=/dev/null bs=1M) 2>&1 )
    read_speed=$(parse_speed "$raw_read")

    echo "$FS_TYPE,read,$read_speed,$size_mb" >> "$RESULTS_FILE"
    echo "    Скорость чтения: ${read_speed} MB/s"
done


# ===== Тест 2: IOPS =====
echo "Тест 2: IOPS (создание 1000 файлов по 1KB)..."

create_time=$(
    /usr/bin/time -f "%e" bash -c '
        for i in $(seq 1 1000); do
            dd if=/dev/zero of="small_$i" bs=1k count=1 >/dev/null 2>&1
        done
    ' 2>&1
)

iops=$(echo "1000 / $create_time" | bc -l)

echo "$FS_TYPE,iops,$iops,0" >> "$RESULTS_FILE"
echo "  Время создания: ${create_time}s"
echo "  IOPS (создание): $iops"


# ===== Тест 3: Latency =====
echo "Тест 3: Задержка (оценка)..."

write_latency_ms=$(echo "$create_time * 1" | bc -l)
echo "$FS_TYPE,latency_write,$write_latency_ms,0" >> "$RESULTS_FILE"
echo "  Средняя задержка записи: ${write_latency_ms} ms"

dd if=/dev/zero of=latency_read_test bs=4k count=1 >/dev/null 2>&1

read_time=$(
    /usr/bin/time -f "%e" bash -c '
        for i in $(seq 1 1000); do
            dd if=latency_read_test of=/dev/null bs=4k count=1 >/dev/null 2>&1
        done
    ' 2>&1
)

read_latency_ms=$(echo "$read_time * 1" | bc -l)
echo "$FS_TYPE,latency_read,$read_latency_ms,0" >> "$RESULTS_FILE"

echo "  Средняя задержка чтения: ${read_latency_ms} ms"


# ===== Очистка =====
echo "Очистка..."
cd "$SCRIPT_DIR"
rm -rf "$TEST_DIR"/*

echo "--- Тестирование завершено ---"
