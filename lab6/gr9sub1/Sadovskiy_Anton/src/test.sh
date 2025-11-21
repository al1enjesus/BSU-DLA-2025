 
#!/bin/bash

# Директории
SOURCE_DIR="/tmp/fuse_test_source"
MOUNT_DIR="/tmp/fuse_test_mount"

echo -e "=== FUSE Filesystem Testing Script ==="
echo ""

# Функция очистки
cleanup() {
    echo -e "Cleaning up..."

    # Размонтировать если смонтировано
    if mountpoint -q "$MOUNT_DIR" 2>/dev/null; then
        fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null
    fi

    # Удалить тестовые директории
    rm -rf "$SOURCE_DIR" "$MOUNT_DIR"
}

# Подготовка
prepare() {
    echo -e "1. Preparing test environment..."
    cleanup

    mkdir -p "$SOURCE_DIR" "$MOUNT_DIR"

    # Создать тестовые файлы
    echo "Hello World" > "$SOURCE_DIR/test.txt"
    echo "lowercase text" > "$SOURCE_DIR/case.txt"
    echo "The Quick Brown Fox Jumps Over The Lazy Dog" > "$SOURCE_DIR/pangram.txt"
    mkdir -p "$SOURCE_DIR/subdir"
    echo "Nested file" > "$SOURCE_DIR/subdir/nested.txt"

    echo -e "Test files created in $SOURCE_DIR"
    ls -la "$SOURCE_DIR"
    echo ""
}

# Тест 1: Passthrough FS
test_passthrough() {
    echo -e "=== Test 1: Passthrough Filesystem ==="

    # Запустить в фоне
    # Один поток
    ./passthrough_fs "$SOURCE_DIR" "$MOUNT_DIR" -f -s &
    PID=$!
    sleep 2

    echo -e "$[TEST] Reading file through FUSE:"
    cat "$MOUNT_DIR/test.txt"

    echo -e "$[TEST] Creating new file:"
    echo "New content" > "$MOUNT_DIR/newfile.txt"
    cat "$MOUNT_DIR/newfile.txt"

    echo -e "$[TEST] Listing mounted directory:"
    ls -la "$MOUNT_DIR"

    # Проверить что файл появился в source
    echo -e "$[TEST] Checking source directory:"
    ls -la "$SOURCE_DIR"

    # Остановить
    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
    fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null

    echo -e "Passthrough test completed!"
    echo ""
}

# Тест 2: ROT13 FS
test_rot13() {
    echo -e "=== Test 2: ROT13 Encryption Filesystem ==="

    # Запустить в фоне
    ./rot13_fs "$SOURCE_DIR" "$MOUNT_DIR" -f &
    PID=$!
    sleep 2

    echo -e "[TEST] Writing encrypted content:"
    echo "Secret Message" > "$MOUNT_DIR/secret.txt"

    echo -e "[TEST] Reading through FUSE (decrypted):"
    cat "$MOUNT_DIR/secret.txt"

    echo -e "[TEST] Reading directly from source (encrypted):"
    cat "$SOURCE_DIR/secret.txt"

    echo -e "[TEST] Verification:"
    echo "Expected: Secret Message"
    echo -n "Got from FUSE: "
    cat "$MOUNT_DIR/secret.txt"
    echo -n "Got from source (encrypted): "
    cat "$SOURCE_DIR/secret.txt"

    # Остановить
    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
    fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null

    echo -e "ROT13 test completed!"
    echo ""
}

# Тест 3: Uppercase FS
test_uppercase() {
    echo -e "=== Test 3: Uppercase Filesystem ==="

    # Запустить в фоне
    ./uppercase_fs "$SOURCE_DIR" "$MOUNT_DIR" -f &
    PID=$!
    sleep 2

    echo -e "[TEST] Reading with uppercase transform:"
    echo -n "Original (source): "
    cat "$SOURCE_DIR/case.txt"
    echo -n "Uppercase (FUSE): "
    cat "$MOUNT_DIR/case.txt"

    echo -e "[TEST] Pangram test:"
    echo -n "Original: "
    cat "$SOURCE_DIR/pangram.txt"
    echo -n "Uppercase: "
    cat "$MOUNT_DIR/pangram.txt"

    echo -e "[TEST] Writing new file:"
    echo "mixed CaSe TeXt" > "$MOUNT_DIR/mixed.txt"
    echo -n "Written (source): "
    cat "$SOURCE_DIR/mixed.txt"
    echo -n "Read back (FUSE): "
    cat "$MOUNT_DIR/mixed.txt"

    # Остановить
    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
    fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null

    echo -e "Uppercase test completed!"
    echo ""
}


# Benchmark тест
benchmark() {
    echo -e "=== Performance Benchmark ==="

    # Создать большой файл для теста
    dd if=/dev/zero of="$SOURCE_DIR/bigfile" bs=1M count=10 2>/dev/null

    echo -e "[BENCHMARK] Direct read from source:"
    time dd if="$SOURCE_DIR/bigfile" of=/dev/null bs=4096 2>&1 | grep -v records

    # Тест с passthrough
    ./passthrough_fs "$SOURCE_DIR" "$MOUNT_DIR" -f &
    PID=$!
    sleep 2

    echo -e "[BENCHMARK] Read through Passthrough FUSE:"
    time dd if="$MOUNT_DIR/bigfile" of=/dev/null bs=4096 2>&1 | grep -v records

    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
    fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null

    # Тест с uppercase
    ./uppercase_fs "$SOURCE_DIR" "$MOUNT_DIR" -f &
    PID=$!
    sleep 2

    echo -e "[BENCHMARK] Read through Uppercase FUSE:"
    time dd if="$MOUNT_DIR/bigfile" of=/dev/null bs=4096 2>&1 | grep -v records

    kill $PID 2>/dev/null
    wait $PID 2>/dev/null
    fusermount -u "$MOUNT_DIR" 2>/dev/null || umount "$MOUNT_DIR" 2>/dev/null

    echo -e "Benchmark completed!"
    echo ""
}

# Главное меню
main() {
    # Проверить что программы собраны
    if [ ! -f "./passthrough_fs" ] || [ ! -f "./rot13_fs" ] || [ ! -f "./uppercase_fs" ]; then
        echo -e "Error: Binaries not found. Run 'make' first."
        exit 1
    fi

    prepare

    case "${1:-all}" in
        passthrough)
            test_passthrough
            ;;
        rot13)
            test_rot13
            ;;
        uppercase)
            test_uppercase
            ;;
        benchmark)
            benchmark
            ;;
        all)
            test_passthrough
            test_rot13
            test_uppercase
            benchmark
            ;;
        *)
            echo "Usage: $0 [passthrough|rot13|uppercase|benchmark|all]"
            exit 1
            ;;
    esac

    echo -e "All tests completed successfully!"
}

# Обработка Ctrl+C
# Закомментил, потому что так проще тестировать ручками
# trap cleanup EXIT

main "$@"
