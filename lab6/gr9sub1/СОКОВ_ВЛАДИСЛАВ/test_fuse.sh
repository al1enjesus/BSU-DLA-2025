#!/bin/bash
# test_fuse.sh

set -e  # Exit on error

MOUNT="/mnt/fuse"

echo "=== Testing FUSE Filesystem ==="

# 1. Создание файла
echo "Test 1: Create file"
echo "Hello" > $MOUNT/test.txt
[ -f $MOUNT/test.txt ] && echo "✓ File created"

# 2. Чтение файла
echo "Test 2: Read file"
CONTENT=$(cat $MOUNT/test.txt)
[ "$CONTENT" = "Hello" ] && echo "✓ Content matches"

# 3. Запись в файл
echo "Test 3: Write to file"
echo "World" >> $MOUNT/test.txt
grep -q "World" $MOUNT/test.txt && echo "✓ Write successful"

# 4. Создание директории
echo "Test 4: Create directory"
mkdir $MOUNT/testdir
[ -d $MOUNT/testdir ] && echo "✓ Directory created"

# 5. Файл в директории
echo "Test 5: File in directory"
echo "Nested" > $MOUNT/testdir/nested.txt
[ -f $MOUNT/testdir/nested.txt ] && echo "✓ Nested file created"

# 6. Список файлов
echo "Test 6: List directory"
ls $MOUNT/ | grep -q "test.txt" && echo "✓ Directory listing works"

# 7. Удаление файла
echo "Test 7: Delete file"
rm $MOUNT/test.txt
[ ! -f $MOUNT/test.txt ] && echo "✓ File deleted"

# 8. Удаление директории
echo "Test 8: Delete directory"
rm $MOUNT/testdir/nested.txt
rmdir $MOUNT/testdir
[ ! -d $MOUNT/testdir ] && echo "✓ Directory deleted"

echo ""
echo "All tests passed! ✓"