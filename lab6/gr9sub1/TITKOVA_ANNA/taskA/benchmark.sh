#!/bin/bash

# Исправленный benchmark.sh

set -e

echo "=== Passthrough FUSE Performance Benchmark ==="
echo ""

# Конфигурация
SOURCE_DIR="/tmp/fuse_bench_source"
MOUNT_DIR="/tmp/fuse_bench_mount"
TEST_FS="./myfuse"  # было: TEST_FS="./passthrough_fuse"
TEST_FILE_SIZE_MB=10  # Уменьшим для теста
TEST_FILE_COUNT=100   # Уменьшим для теста
REPEAT_COUNT=2        # Уменьшим количество повторов

# Функция для измерения времени
time_command() {
    local cmd="$1"
    local desc="$2"
    
    echo -n "  $desc: "
    /usr/bin/time -f "%e seconds" bash -c "$cmd" 2>&1 | tail -1
}

# Очистка
cleanup() {
    echo "Cleaning up..."
    fusermount -u "$MOUNT_DIR" 2>/dev/null || true
    rm -rf "$SOURCE_DIR" "$MOUNT_DIR" 2>/dev/null || true
    pkill -f "$TEST_FS" 2>/dev/null || true
    sleep 1
}

# Настройка
setup() {
    cleanup
    mkdir -p "$SOURCE_DIR" "$MOUNT_DIR"
    
    if [ ! -f "$TEST_FS" ]; then
        make
    fi
    
    echo "Starting FUSE filesystem..."
    "$TEST_FS" "$SOURCE_DIR" "$MOUNT_DIR" -f &
    FUSE_PID=$!
    sleep 3
    
    # Проверка монтирования
    if ! mountpoint -q "$MOUNT_DIR"; then
        echo "ERROR: Failed to mount FUSE filesystem"
        exit 1
    fi
    echo "FUSE mounted successfully"
}

# Основной бенчмарк
main() {
    echo "Setting up benchmark environment..."
    setup
    
    echo -e "\n=== 1. Sequential Write Test ==="
    echo "Creating $TEST_FILE_SIZE_MB MB file..."
    rm -f "$MOUNT_DIR/large_file.bin" 2>/dev/null || true
    time_command "dd if=/dev/zero of='$MOUNT_DIR/large_file.bin' bs=1M count=$TEST_FILE_SIZE_MB conv=fdatasync status=none" "Write"
    
    echo -e "\n=== 2. Sequential Read Test ==="
    time_command "dd if='$MOUNT_DIR/large_file.bin' of=/dev/null bs=1M status=none" "Read"
    
    echo -e "\n=== 3. Random Access Test ==="
    time_command "dd if='$MOUNT_DIR/large_file.bin' of=/dev/null bs=4K count=1000 iflag=direct status=none" "Random read (4K blocks)"
    
    echo -e "\n=== 4. Small Files Creation ==="
    echo "Creating $TEST_FILE_COUNT small files..."
    rm -f "$MOUNT_DIR"/test_file_* 2>/dev/null || true
    time_command "
        for i in \$(seq 1 $TEST_FILE_COUNT); do
            echo 'Test content \$i' > '$MOUNT_DIR/test_file_\$i.txt'
        done
    " "Create $TEST_FILE_COUNT files"
    
    echo -e "\n=== 5. Metadata Operations ==="
    time_command "
        for i in \$(seq 1 100); do
            stat '$MOUNT_DIR/test_file_1.txt' > /dev/null
        done
    " "Stat 100 times"
    
    time_command "
        for i in \$(seq 1 100); do
            touch '$MOUNT_DIR/test_file_1.txt'
        done
    " "Touch 100 times"
    
    echo -e "\n=== 6. Directory Operations ==="
    time_command "
        for i in \$(seq 1 50); do
            mkdir -p '$MOUNT_DIR/test_dir_\$i/subdir'
        done
    " "Create 50 directories"
    
    time_command "
        for i in \$(seq 1 50); do
            ls -la '$MOUNT_DIR/test_dir_\$i' > /dev/null
        done
    " "List 50 directories"
    
    echo -e "\n=== 7. File Operations ==="
    time_command "
        for i in \$(seq 1 50); do
            cp '$MOUNT_DIR/test_file_1.txt' '$MOUNT_DIR/test_file_copy_\$i.txt'
        done
    " "Copy file 50 times"
    
    time_command "
        for i in \$(seq 1 50); do
            mv '$MOUNT_DIR/test_file_copy_\$i.txt' '$MOUNT_DIR/test_file_moved_\$i.txt'
        done
    " "Rename file 50 times"
    
    # Очистка тестовых файлов
    echo -e "\n=== Cleaning up test files ==="
    rm -rf "$MOUNT_DIR"/test_* 2>/dev/null || true
    rm -f "$MOUNT_DIR/large_file.bin" 2>/dev/null || true
    
    # Сравнение с прямой файловой системой
    echo -e "\n=== 8. Comparison with Direct FS ==="
    echo "Direct filesystem write:"
    time_command "dd if=/dev/zero of='$SOURCE_DIR/direct.bin' bs=1M count=$TEST_FILE_SIZE_MB conv=fdatasync status=none" "Direct write"
    
    echo -e "\nDirect filesystem read:"
    time_command "dd if='$SOURCE_DIR/direct.bin' of=/dev/null bs=1M status=none" "Direct read"
    
    # Вывод статистики
    echo -e "\n=== Final Statistics ==="
    echo "To see FUSE statistics, send SIGINT to process $FUSE_PID or check stderr"
    
    # Запрос на размонтирование
    echo -e "\nPress Enter to unmount and exit..."
    read
    
    cleanup
    
    echo -e "\n${GREEN}=== Benchmark completed! ==="
    echo "Summary:"
    echo "- All operations completed successfully"
    echo "- Check timing results above"
    echo "- Compare FUSE overhead vs direct filesystem"
}

# Обработка прерывания
trap 'echo -e "\nInterrupted!"; cleanup; exit 1' INT TERM

# Запуск
main "$@"
