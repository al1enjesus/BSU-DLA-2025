#!/bin/bash

set -e

echo "=== FUSE Filesystems Test Script ==="
echo ""

# Подготовка директорий
mkdir -p /tmp/source /tmp/fuse
rm -rf /tmp/source/* /tmp/fuse/*

echo "1. Testing Passthrough FS..."
timeout 3s ./passthrough_fs /tmp/source /tmp/fuse -f 2>&1 | head -10 &
PID=$!
sleep 1

echo "  - Creating and writing file..."
echo "Hello Passthrough FS" > /tmp/fuse/test_pt.txt
echo "  - Reading through FUSE:"
cat /tmp/fuse/test_pt.txt
echo "  - Reading from source (should be identical):"
cat /tmp/source/test_pt.txt

kill $PID 2>/dev/null || true
fusermount -uz /tmp/fuse 2>/dev/null || true
sleep 1

echo ""
echo "=== All tests completed! ==="