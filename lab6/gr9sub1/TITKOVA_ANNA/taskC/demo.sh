#!/bin/bash
# demo.sh - Простой демонстрационный тест Monitoring FUSE

set -e

echo "=== Testing Monitoring FUSE ==="

# Удаляем старые тестовые данные
echo "Cleaning up..."
fusermount -u /tmp/fuse_mount 2>/dev/null || true
rm -rf /tmp/my_source /tmp/fuse_mount 2>/dev/null

# Создаем директории
echo "Creating directories..."
mkdir -p /tmp/my_source
mkdir -p /tmp/fuse_mount

# Добавляем тестовый файл
echo "Creating test file..."
echo "Hello from source" > /tmp/my_source/test.txt

# Компилируем
echo "Building..."
make clean
make

echo -e "\n--- Starting FUSE filesystem ---"
echo "Source: /tmp/my_source"
echo "Mount: /tmp/fuse_mount"

# Запускаем в фоне без дебага (меньше вывода)
./monitoring_fuse /tmp/my_source /tmp/fuse_mount -f &
FS_PID=$!

# Ждем монтирования
sleep 2

echo -e "\n--- Testing ---"

# Проверяем что точка монтирования видна
if mount | grep -q "/tmp/fuse_mount"; then
    echo "✓ Filesystem mounted successfully"
else
    echo "✗ Filesystem not mounted!"
    kill $FS_PID 2>/dev/null || true
    exit 1
fi

# Тестируем операции
echo "1. Listing files:"
ls -la /tmp/fuse_mount/

echo -e "\n2. Reading stats file:"
cat /tmp/fuse_mount/.stats

echo -e "\n3. Reading existing file:"
cat /tmp/fuse_mount/test.txt

echo -e "\n4. Creating new file:"
echo "New content" > /tmp/fuse_mount/newfile.txt

echo -e "\n5. Creating directory:"
mkdir /tmp/fuse_mount/testdir

echo -e "\n6. Writing to new file:"
echo "File in subdir" > /tmp/fuse_mount/testdir/file.txt

echo -e "\n7. Reading updated stats:"
cat /tmp/fuse_mount/.stats

echo -e "\n8. Checking source directory:"
ls -la /tmp/my_source/

echo -e "\n9. Unmounting..."
fusermount -u /tmp/fuse_mount

# Ждем завершения процесса
wait $FS_PID 2>/dev/null || true

echo -e "\n=== Test completed successfully ==="
