#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

cleanup() {
    log "Очистка..."
    fusermount3 -u /tmp/opers_mnt 2>/dev/null || true
    fusermount3 -u /tmp/archive_mnt 2>/dev/null || true
    fusermount3 -u /tmp/mon_mnt 2>/dev/null || true
    sleep 1
}

trap cleanup EXIT

if [ ! -f "build/operations" ] || [ ! -f "build/archive" ] || [ ! -f "build/monitoring" ]; then
    error "Бинарные файлы не найдены. Сначала выполните 'make all'."
    exit 1
fi

echo "=========================================="
echo "    Тестирование FUSE файловых систем"
echo "=========================================="
echo ""

log "Тест 1: Файловая система Passthrough"
echo "------------------------------------------"

mkdir -p /tmp/opers_source /tmp/opers_mnt

log "Монтирование файловой системы passthrough..."
./build/operations /tmp/opers_source /tmp/opers_mnt -f 2>/tmp/opers_log.txt &
OPERS_PID=$!
sleep 2

if ! mountpoint -q /tmp/opers_mnt; then
    error "Не удалось смонтировать passthrough FS"
    kill $OPERS_PID 2>/dev/null || true
    exit 1
fi

log "Создание файла..."
echo "Hello FUSE World!" > /tmp/opers_mnt/test.txt

log "Чтение файла..."
cat /tmp/opers_mnt/test.txt

log "Создание директории..."
mkdir /tmp/opers_mnt/testdir

log "Создание файла в директории..."
echo "Nested file" > /tmp/opers_mnt/testdir/nested.txt

log "Содержимое директории..."
ls -la /tmp/opers_mnt/

log "Чтение вложенного файла..."
cat /tmp/opers_mnt/testdir/nested.txt

log "Удаление файла..."
rm /tmp/opers_mnt/test.txt

log "Удаление директории..."
rm -rf /tmp/opers_mnt/testdir

log "Проверка логов..."
tail -20 /tmp/opers_log.txt

log "Отмонтирование..."
fusermount3 -u /tmp/opers_mnt
kill $OPERS_PID 2>/dev/null || true
wait $OPERS_PID 2>/dev/null || true

log "Тест 1 ПРОЙДЕН ✓"
echo ""

log "Тест 2: Архивная файловая система (только чтение)"
echo "------------------------------------------"

mkdir -p /tmp/test_archive/subdir1/subdir2
echo "Root file content" > /tmp/test_archive/root.txt
echo "File in subdir1" > /tmp/test_archive/subdir1/file1.txt
echo "File in subdir2" > /tmp/test_archive/subdir1/subdir2/file2.txt
echo "Another root file" > /tmp/test_archive/data.txt

tar -cf /tmp/test.tar -C /tmp/test_archive .

log "Архив создан. Содержимое:"
tar -tf /tmp/test.tar

mkdir -p /tmp/archive_mnt

log "Монтирование архивной файловой системы..."
./build/archive /tmp/test.tar /tmp/archive_mnt -f 2>/tmp/archive_log.txt &
ARCH_PID=$!
sleep 2

if ! mountpoint -q /tmp/archive_mnt; then
    error "Не удалось смонтировать архив FS"
    kill $ARCH_PID 2>/dev/null || true
    exit 1
fi

log "Содержимое корня..."
ls -la /tmp/archive_mnt/

log "Чтение файла в корне..."
cat /tmp/archive_mnt/root.txt

log "Содержимое поддиректории..."
ls -la /tmp/archive_mnt/subdir1/

log "Чтение файла из поддиректории..."
cat /tmp/archive_mnt/subdir1/file1.txt

log "Чтение вложенного файла..."
cat /tmp/archive_mnt/subdir1/subdir2/file2.txt

log "Проверка только для чтения..."
if echo "test" > /tmp/archive_mnt/test.txt 2>/dev/null; then
    error "Архивная FS позволила запись (должна быть только для чтения)!"
else
    log "Запись корректно запрещена ✓"
fi

log "Отмонтирование..."
fusermount3 -u /tmp/archive_mnt
kill $ARCH_PID 2>/dev/null || true
wait $ARCH_PID 2>/dev/null || true

rm -rf /tmp/test_archive /tmp/test.tar

log "Тест 2 ПРОЙДЕН ✓"
echo ""

log "Тест 3: Мониторинг с файловой системой статистики"
echo "------------------------------------------"

mkdir -p /tmp/mon_source /tmp/mon_mnt

log "Монтирование мониторинговой FS..."
./build/monitoring /tmp/mon_source /tmp/mon_mnt -f 2>/tmp/mon_log.txt &
MON_PID=$!
sleep 2

if ! mountpoint -q /tmp/mon_mnt; then
    error "Не удалось смонтировать мониторинговую FS"
    kill $MON_PID 2>/dev/null || true
    exit 1
fi

log "Начальная статистика:"
cat /tmp/mon_mnt/.stats
echo ""

log "Выполнение операций..."
echo "Data 1" > /tmp/mon_mnt/file1.txt
echo "Data 2" > /tmp/mon_mnt/file2.txt
echo "Data 3" > /tmp/mon_mnt/file3.txt

cat /tmp/mon_mnt/file1.txt > /dev/null
cat /tmp/mon_mnt/file2.txt > /dev/null
cat /tmp/mon_mnt/file3.txt > /dev/null

mkdir /tmp/mon_mnt/testdir
echo "Nested" > /tmp/mon_mnt/testdir/nested.txt

ls -la /tmp/mon_mnt/ > /dev/null

log "Статистика после операций:"
cat /tmp/mon_mnt/.stats
echo ""

log "Дополнительные операции..."
for i in {1..5}; do
    echo "Test $i" > /tmp/mon_mnt/test$i.txt
    cat /tmp/mon_mnt/test$i.txt > /dev/null
done

log "Итоговая статистика:"
cat /tmp/mon_mnt/.stats
echo ""

log "Проверка ненулевых значений статистики..."
READS=$(grep "^reads:" /tmp/mon_mnt/.stats | awk '{print $2}')
WRITES=$(grep "^writes:" /tmp/mon_mnt/.stats | awk '{print $2}')
BYTES_READ=$(grep "^bytes_read:" /tmp/mon_mnt/.stats | awk '{print $2}')
BYTES_WRITTEN=$(grep "^bytes_written:" /tmp/mon_mnt/.stats | awk '{print $2}')

if [ "$READS" -gt 0 ] && [ "$WRITES" -gt 0 ] && [ "$BYTES_READ" -gt 0 ] && [ "$BYTES_WRITTEN" -gt 0 ]; then
    log "Статистика корректно ведётся ✓"
else
    error "Статистика ведётся некорректно!"
    exit 1
fi

log "Отмонтирование..."
fusermount3 -u /tmp/mon_mnt
kill $MON_PID 2>/dev/null || true
wait $MON_PID 2>/dev/null || true

log "Тест 3 ПРОЙДЕН ✓"
echo ""

echo "=========================================="
echo "           ВСЕ ТЕСТЫ ПРОЙДЕНЫ ✓"
echo "=========================================="
echo ""
log "Задача A: Operations - ОК"
log "Задача B: Archive (только чтение) - ОК"
log "Задача C: Monitoring - ОК"
echo ""

exit 0