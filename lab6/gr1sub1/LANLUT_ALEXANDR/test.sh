set -e

echo "=== Quick Test for FUSE Filesystems ==="
echo ""

# Подготовка
mkdir -p /tmp/source /tmp/fuse
rm -rf /tmp/source/* /tmp/fuse/*

echo "1. Testing Passthrough FS..."
cd src/
timeout 2s ./passthrough /tmp/source /tmp/fuse -f 2>&1 | head -5 &
PID=$!
sleep 0.5

echo "  - Creating file..."
echo "Hello Passthrough" > /tmp/fuse/test1.txt
echo "  - Reading file..."
cat /tmp/fuse/test1.txt
echo "  - Checking source directory..."
cat /tmp/source/test1.txt

kill $PID 2>/dev/null || true
fusermount -uz /tmp/fuse 2>/dev/null || true
sleep 0.5

echo ""
echo "2. Testing ROT13 FS..."
rm -rf /tmp/source/* /tmp/fuse/*
timeout 2s ./rot13_fs /tmp/source /tmp/fuse -f 2>&1 | head -5 &
PID=$!
sleep 0.5

echo "  - Writing encrypted data..."
echo "Hello ROT13" > /tmp/fuse/secret.txt
echo "  - Reading through FUSE (decrypted):"
cat /tmp/fuse/secret.txt
echo "  - Reading from disk (encrypted):"
cat /tmp/source/secret.txt

kill $PID 2>/dev/null || true
fusermount -uz /tmp/fuse 2>/dev/null || true
sleep 0.5

echo ""
echo "3. Testing Uppercase FS..."
rm -rf /tmp/source/* /tmp/fuse/*
timeout 2s ./uppercase_fs /tmp/source /tmp/fuse -f 2>&1 | head -5 &
PID=$!
sleep 0.5

echo "  - Writing lowercase data..."
echo "hello uppercase" > /tmp/fuse/test3.txt
echo "  - Reading through FUSE (uppercase):"
cat /tmp/fuse/test3.txt
echo "  - Reading from disk (original):"
cat /tmp/source/test3.txt

kill $PID 2>/dev/null || true
fusermount -uz /tmp/fuse 2>/dev/null || true

echo ""
echo "=== All tests completed successfully! ==="
