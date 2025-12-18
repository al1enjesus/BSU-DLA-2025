#!/bin/bash
# benchmark.sh - Нагрузочное тестирование Monitoring FUSE

set -e

echo "=== Monitoring FUSE Load Testing ==="

# Используем домашнюю директорию, чтобы избежать проблем с /tmp
TEST_DIR="$HOME/fuse_benchmark_test"
SOURCE_DIR="$TEST_DIR/source"
MOUNT_DIR="$TEST_DIR/mount"

# Очистка
echo "Cleaning up old test directories..."
fusermount -u "$MOUNT_DIR" 2>/dev/null || true
rm -rf "$TEST_DIR" 2>/dev/null

# Создание тестовых директорий
echo "Creating test directories..."
mkdir -p "$SOURCE_DIR"
mkdir -p "$MOUNT_DIR"

echo "Source directory: $SOURCE_DIR"
echo "Mount directory: $MOUNT_DIR"

# Компиляция
echo "Building..."
make clean
make

echo -e "\n--- Starting filesystem ---"
./monitoring_fuse "$SOURCE_DIR" "$MOUNT_DIR" -f &
FS_PID=$!

# Даем время на монтирование
sleep 3

# Проверка, что процесс жив
if ! kill -0 $FS_PID 2>/dev/null; then
    echo "✗ Filesystem process died!"
    echo "Check if fuse is installed: ls -la /dev/fuse"
    echo "Check permissions: ls -la $MOUNT_DIR"
    exit 1
fi

# Проверка монтирования
if mount | grep -q "$MOUNT_DIR"; then
    echo "✓ Filesystem mounted successfully"
else
    echo "✗ Filesystem not mounted!"
    kill $FS_PID 2>/dev/null || true
    exit 1
fi

# Функция для измерения времени
measure_time() {
    local desc="$1"
    shift
    echo -n "$desc: "
    /usr/bin/time -f "%e seconds" "$@" 2>&1 | tail -1
}

echo -e "\n--- Test 1: Small file operations (100 files) ---"
measure_time "Create 100 files" bash -c "for i in {1..100}; do echo 'test' > \"$MOUNT_DIR/file_\$i.txt\"; done"
measure_time "Read 100 files" bash -c "for i in {1..100}; do cat \"$MOUNT_DIR/file_\$i.txt\" > /dev/null; done"
measure_time "Delete 100 files" bash -c "for i in {1..100}; do rm \"$MOUNT_DIR/file_\$i.txt\"; done"

echo -e "\n--- Test 2: Sequential read/write ---"
echo "Creating 1MB test file..."
dd if=/dev/zero of="$MOUNT_DIR/largefile" bs=1M count=1 2>/dev/null

measure_time "Sequential read 1MB" dd if="$MOUNT_DIR/largefile" of=/dev/null bs=1M 2>/dev/null
measure_time "Sequential write 1MB" dd if=/dev/zero of="$MOUNT_DIR/write_test" bs=1M count=1 2>/dev/null

echo -e "\n--- Test 3: Directory operations ---"
measure_time "Create 50 directories" bash -c "for i in {1..50}; do mkdir \"$MOUNT_DIR/dir_\$i\"; done"
measure_time "Remove 50 directories" bash -c "for i in {1..50}; do rmdir \"$MOUNT_DIR/dir_\$i\"; done"

echo -e "\n--- Test 4: Virtual stats file ---"
echo "Reading statistics 10 times:"
time for i in {1..10}; do
    cat "$MOUNT_DIR/.stats" > /dev/null
done

echo -e "\n--- Test 5: Mixed operations ---"
measure_time "Create/read/delete 50 files" bash -c "
    for i in {1..50}; do
        echo 'data \$i' > \"$MOUNT_DIR/mixed_\$i.txt\"
        cat \"$MOUNT_DIR/mixed_\$i.txt\" > /dev/null
        rm \"$MOUNT_DIR/mixed_\$i.txt\"
    done
"

echo -e "\n=== Final Statistics ==="
cat "$MOUNT_DIR/.stats"

# Очистка
echo -e "\n--- Cleaning up ---"
fusermount -u "$MOUNT_DIR" 2>/dev/null || kill $FS_PID 2>/dev/null || true
wait $FS_PID 2>/dev/null || true

rm -rf "$TEST_DIR" 2>/dev/null || true

echo -e "\n=== Benchmark completed successfully! ==="
