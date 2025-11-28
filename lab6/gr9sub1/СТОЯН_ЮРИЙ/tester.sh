set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

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
    log "Cleaning up..."
    fusermount3 -u /tmp/pass_mnt 2>/dev/null || true
    fusermount3 -u /tmp/archive_mnt 2>/dev/null || true
    fusermount3 -u /tmp/mon_mnt 2>/dev/null || true
    sleep 1
}

trap cleanup EXIT

if [ ! -f "build/passthrough" ] || [ ! -f "build/archive" ] || [ ! -f "build/monitoring" ]; then
    error "Binaries not found. Run 'make all' first."
    exit 1
fi

echo "=========================================="
echo "    FUSE Filesystem Lab - Tests"
echo "=========================================="
echo ""

log "Test 1: Passthrough Filesystem"
echo "------------------------------------------"

mkdir -p /tmp/pass_source /tmp/pass_mnt

log "Mounting passthrough filesystem..."
./build/passthrough /tmp/pass_source /tmp/pass_mnt -f 2>/tmp/pass_log.txt &
PASS_PID=$!
sleep 2

if ! mountpoint -q /tmp/pass_mnt; then
    error "Failed to mount passthrough filesystem"
    kill $PASS_PID 2>/dev/null || true
    exit 1
fi

log "Creating file..."
echo "Hello FUSE World!" > /tmp/pass_mnt/test.txt

log "Reading file..."
cat /tmp/pass_mnt/test.txt

log "Creating directory..."
mkdir /tmp/pass_mnt/testdir

log "Creating file in directory..."
echo "Nested file" > /tmp/pass_mnt/testdir/nested.txt

log "Listing directory..."
ls -la /tmp/pass_mnt/

log "Reading nested file..."
cat /tmp/pass_mnt/testdir/nested.txt

log "Deleting file..."
rm /tmp/pass_mnt/test.txt

log "Removing directory..."
rm -rf /tmp/pass_mnt/testdir

log "Checking logs..."
tail -20 /tmp/pass_log.txt

log "Unmounting..."
fusermount3 -u /tmp/pass_mnt
kill $PASS_PID 2>/dev/null || true
wait $PASS_PID 2>/dev/null || true

log "Test 1 PASSED ✓"
echo ""

log "Test 2: Archive Filesystem (read-only)"
echo "------------------------------------------"

log "Creating test archive..."
mkdir -p /tmp/test_archive/subdir1/subdir2
echo "Root file content" > /tmp/test_archive/root.txt
echo "File in subdir1" > /tmp/test_archive/subdir1/file1.txt
echo "File in subdir2" > /tmp/test_archive/subdir1/subdir2/file2.txt
echo "Another root file" > /tmp/test_archive/data.txt

tar -cf /tmp/test.tar -C /tmp/test_archive .

log "Archive created. Contents:"
tar -tf /tmp/test.tar

mkdir -p /tmp/archive_mnt

log "Mounting archive filesystem..."
./build/archive /tmp/test.tar /tmp/archive_mnt -f 2>/tmp/archive_log.txt &
ARCH_PID=$!
sleep 2

if ! mountpoint -q /tmp/archive_mnt; then
    error "Failed to mount archive filesystem"
    kill $ARCH_PID 2>/dev/null || true
    exit 1
fi

log "Listing root directory..."
ls -la /tmp/archive_mnt/

log "Reading root file..."
cat /tmp/archive_mnt/root.txt

log "Listing subdirectory..."
ls -la /tmp/archive_mnt/subdir1/

log "Reading file from subdirectory..."
cat /tmp/archive_mnt/subdir1/file1.txt

log "Reading deeply nested file..."
cat /tmp/archive_mnt/subdir1/subdir2/file2.txt

log "Testing read-only (should fail)..."
if echo "test" > /tmp/archive_mnt/test.txt 2>/dev/null; then
    error "Archive filesystem allowed write (should be read-only)!"
else
    log "Write correctly denied (read-only) ✓"
fi

log "Unmounting..."
fusermount3 -u /tmp/archive_mnt
kill $ARCH_PID 2>/dev/null || true
wait $ARCH_PID 2>/dev/null || true

rm -rf /tmp/test_archive /tmp/test.tar

log "Test 2 PASSED ✓"
echo ""

log "Test 3: Monitoring Filesystem with Statistics"
echo "------------------------------------------"

mkdir -p /tmp/mon_source /tmp/mon_mnt

log "Mounting monitoring filesystem..."
./build/monitoring /tmp/mon_source /tmp/mon_mnt -f 2>/tmp/mon_log.txt &
MON_PID=$!
sleep 2

if ! mountpoint -q /tmp/mon_mnt; then
    error "Failed to mount monitoring filesystem"
    kill $MON_PID 2>/dev/null || true
    exit 1
fi

log "Initial statistics:"
cat /tmp/mon_mnt/.stats
echo ""

log "Performing operations..."
echo "Data 1" > /tmp/mon_mnt/file1.txt
echo "Data 2" > /tmp/mon_mnt/file2.txt
echo "Data 3" > /tmp/mon_mnt/file3.txt

cat /tmp/mon_mnt/file1.txt > /dev/null
cat /tmp/mon_mnt/file2.txt > /dev/null
cat /tmp/mon_mnt/file3.txt > /dev/null

mkdir /tmp/mon_mnt/testdir
echo "Nested" > /tmp/mon_mnt/testdir/nested.txt

ls -la /tmp/mon_mnt/ > /dev/null

log "Statistics after operations:"
cat /tmp/mon_mnt/.stats
echo ""

log "More operations..."
for i in {1..5}; do
    echo "Test $i" > /tmp/mon_mnt/test$i.txt
    cat /tmp/mon_mnt/test$i.txt > /dev/null
done

log "Final statistics:"
cat /tmp/mon_mnt/.stats
echo ""

log "Verifying statistics are non-zero..."
READS=$(grep "^reads:" /tmp/mon_mnt/.stats | awk '{print $2}')
WRITES=$(grep "^writes:" /tmp/mon_mnt/.stats | awk '{print $2}')
BYTES_READ=$(grep "^bytes_read:" /tmp/mon_mnt/.stats | awk '{print $2}')
BYTES_WRITTEN=$(grep "^bytes_written:" /tmp/mon_mnt/.stats | awk '{print $2}')

if [ "$READS" -gt 0 ] && [ "$WRITES" -gt 0 ] && [ "$BYTES_READ" -gt 0 ] && [ "$BYTES_WRITTEN" -gt 0 ]; then
    log "Statistics tracking working correctly ✓"
else
    error "Statistics not tracking correctly!"
    exit 1
fi

log "Unmounting..."
fusermount3 -u /tmp/mon_mnt
kill $MON_PID 2>/dev/null || true
wait $MON_PID 2>/dev/null || true

log "Test 3 PASSED ✓"
echo ""


echo ""
log "Task A: Passthrough FS - OK"
log "Task B: Archive FS (read-only) - OK"
log "Task C: Monitoring FS - OK"
echo ""

exit 0