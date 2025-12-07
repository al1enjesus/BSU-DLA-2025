#!/bin/bash

set -e

echo "=== Quick Test for FUSE Filesystems ==="
echo ""

# Подготовка директорий
SOURCE_DIR="test_dir/source"
MOUNT_DIR="test_dir/mount"
mkdir -p $SOURCE_DIR $MOUNT_DIR

# Функция для безопасного размонтирования
safe_unmount() {
    if mountpoint -q $MOUNT_DIR; then
        fusermount -uz $MOUNT_DIR 2>/dev/null || true
        sleep 1
    fi
}

# Очистка перед началом
safe_unmount
rm -rf $SOURCE_DIR/*

echo "1. Testing Passthrough FS..."
./bin/passthrough_fs $SOURCE_DIR $MOUNT_DIR -f -s &
PASSTHROUGH_PID=$!
sleep 2

echo "  - Creating file..."
echo "Hello Passthrough" > $MOUNT_DIR/test1.txt
echo "  - Reading file through FUSE:"
cat $MOUNT_DIR/test1.txt
echo "  - Checking source directory (should be same):"
cat $SOURCE_DIR/test1.txt

echo "  - Testing directory listing:"
ls -la $MOUNT_DIR/

echo "  - Testing file deletion:"
rm $MOUNT_DIR/test1.txt
ls -la $MOUNT_DIR/

kill $PASSTHROUGH_PID 2>/dev/null || true
safe_unmount
sleep 2

echo ""
echo "2. Testing ROT13 FS..."
rm -rf $SOURCE_DIR/*
./bin/rot13_fs $SOURCE_DIR $MOUNT_DIR -f -s &
ROT13_PID=$!
sleep 2

echo "  - Writing data (will be encrypted)..."
echo "Hello World" > $MOUNT_DIR/secret.txt
echo "  - Reading through FUSE (decrypted):"
cat $MOUNT_DIR/secret.txt
echo "  - Reading from disk (encrypted ROT13):"
cat $SOURCE_DIR/secret.txt

echo "  - Testing ROT13 with special characters..."
echo "Test123!@# ABC xyz" > $MOUNT_DIR/test2.txt
echo "FUSE read:"
cat $MOUNT_DIR/test2.txt
echo "Disk content:"
cat $SOURCE_DIR/test2.txt

kill $ROT13_PID 2>/dev/null || true
safe_unmount
sleep 2

echo ""
echo "3. Testing Uppercase FS..."
rm -rf $SOURCE_DIR/*
./bin/uppercase_fs $SOURCE_DIR $MOUNT_DIR -f -s &
UPPERCASE_PID=$!
sleep 2

echo "  - Writing lowercase data..."
echo "hello world 123 test" > $MOUNT_DIR/test3.txt
echo "  - Reading through FUSE (uppercase):"
cat $MOUNT_DIR/test3.txt
echo "  - Reading from disk (original case):"
cat $SOURCE_DIR/test3.txt

echo "  - Testing mixed case..."
echo "Hello World Mixed CASE" > $MOUNT_DIR/test4.txt
echo "FUSE read (all uppercase):"
cat $MOUNT_DIR/test4.txt
echo "Disk content (original):"
cat $SOURCE_DIR/test4.txt

kill $UPPERCASE_PID 2>/dev/null || true
safe_unmount

echo ""
echo "=== All tests completed successfully! ==="
echo "Check stderr output for operation logs"