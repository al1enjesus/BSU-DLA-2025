#!/usr/bin/env bash
# run_bench.sh
# Запускает бенчмарки: latency per-op, throughput (1/10/100 MB), IOPS (1000 small files)
#
# Usage: ./run_bench.sh <fuse_mount> <ext4_dir> [iterations]
# Example: ./run_bench.sh /mnt/fuse /tmp/source 500
#
# Требования: GNU coreutils (date +%s%3N), bash, python3 (для ascii_graph.py)
#
set -euo pipefail

BASEDIR=$(dirname "$0")
source "$BASEDIR/bench_utils.sh"

if [ $# -lt 2 ]; then
  echo "Usage: $0 <fuse_mount> <ext4_dir> [iterations]"
  exit 1
fi

FUSE_MNT="$1"
EXT4_DIR="$2"
ITER=${3:-500}   # число итераций для latency теста (по умолчанию 500)

OUTDIR="${BASEDIR}/results"
mkdir -p "$OUTDIR"

CSV="$OUTDIR/results.csv"
echo "test,fs,value,unit" > "$CSV"

# Общие параметры
LAT_ITER=$ITER
READ_SIZE=4096
WRITE_SIZE=4096

# Подготовка: создаём тест файлы
echo "Preparing test files..."
mkdir -p "$FUSE_MNT" "$EXT4_DIR"

# ensure example files exist
echo "fuse test content" > "$FUSE_MNT/bench_test_file.bin"
echo "ext4 test content" > "$EXT4_DIR/bench_test_file.bin"

# Функция для записи строки в CSV
write_csv() {
  local test=$1 fs=$2 value=$3 unit=$4
  echo "$test,$fs,$value,$unit" >> "$CSV"
}

# 1) Latency tests per-operation
# Operations: open (open+close), getattr (stat), read 4KB, write 4KB
do_latency() {
  local target=$1 fsname=$2
  log "Latency tests on $fsname -> $target (iterations: $LAT_ITER)"
  # ensure target file exists
  local file="$target/bench_test_file.bin"
  if [ ! -e "$file" ]; then
    dd if=/dev/zero of="$file" bs=4K count=1 2>/dev/null || true
  fi

  # Open: use bash builtin to directly call open syscall
  total=0
  for i in $(seq 1 $LAT_ITER); do
    ms=$(measure_ms bash -c "exec 3<\"$file\"; exec 3>&-")
    total=$(int_add $total $ms)
  done
  avg_ms=$(awk "BEGIN{printf \"%.6f\", $total / $LAT_ITER}")
  write_csv "open" "$fsname" "$avg_ms" "ms"

  # Getattr: stat (используем повторения для точности)
  avg_ms=$(measure_ms_repeated 100 stat -c "%s" "$file")
  write_csv "getattr" "$fsname" "$avg_ms" "ms"

  # Read 4KB
  total=0
  for i in $(seq 1 $LAT_ITER); do
    ms=$(measure_ms dd if="$file" bs=4096 count=1 of=/dev/null 2>/dev/null)
    total=$(int_add $total $ms)
  done
  avg_ms=$(awk "BEGIN{printf \"%.6f\", $total / $LAT_ITER}")
  write_csv "read_4k" "$fsname" "$avg_ms" "ms"

  # Write 4KB (overwrite, non-append)
  # prepare a temp file to avoid hole issues
  tmp="$target/bench_write_tmp.bin"
  dd if=/dev/zero of="$tmp" bs=4096 count=1 2>/dev/null || true
  total=0
  for i in $(seq 1 $LAT_ITER); do
    ms=$(measure_ms dd if=/dev/zero of="$tmp" bs=4096 count=1 conv=notrunc 2>/dev/null)
    total=$(int_add $total $ms)
  done
  avg_ms=$(awk "BEGIN{printf \"%.6f\", $total / $LAT_ITER}")
  write_csv "write_4k" "$fsname" "$avg_ms" "ms"
  # cleanup temp
  rm -f "$tmp"
}

# 2) Throughput tests: create large files and measure MB/s (1MB, 10MB, 100MB)
do_throughput() {
  local target=$1 fsname=$2
  log "Throughput tests on $fsname -> $target"
  for size_mb in 1 10 100; do
    local out="$target/bench_bigfile_${size_mb}MB.bin"
    # remove if exists
    rm -f "$out"
    start=$(now_ms)
    dd if=/dev/zero of="$out" bs=1M count=$size_mb oflag=direct 2>/dev/null || dd if=/dev/zero of="$out" bs=1M count=$size_mb 2>/dev/null
    end=$(now_ms)
    ms=$((end - start))
    # MB/s = size_mb / (ms/1000)
    if [ $ms -eq 0 ]; then
      mbps=0
    else
      mbps=$(awk "BEGIN{printf \"%.3f\", $size_mb / ($ms/1000.0)}")
    fi
    write_csv "throughput_write_${size_mb}MB" "$fsname" "$mbps" "MB/s"

    # read throughput
    start=$(now_ms)
    dd if="$out" of=/dev/null bs=1M iflag=direct 2>/dev/null || dd if="$out" of=/dev/null bs=1M 2>/dev/null
    end=$(now_ms)
    ms=$((end - start))
    if [ $ms -eq 0 ]; then
      mbps=0
    else
      mbps=$(awk "BEGIN{printf \"%.3f\", $size_mb / ($ms/1000.0)}")
    fi
    write_csv "throughput_read_${size_mb}MB" "$fsname" "$mbps" "MB/s"
    rm -f "$out"
  done
}

# 3) IOPS test: create 1000 files of ~1KB and measure time for create & delete
do_iops() {
  local target=$1 fsname=$2
  log "IOPS test on $fsname -> $target"
  local N=1000
  local dir="$target/bench_many_small"
  rm -rf "$dir"
  mkdir -p "$dir"

  # Create N files
  start=$(now_ms)
  for i in $(seq 1 $N); do
    echo "file $i" > "$dir/f$i.txt"
  done
  end=$(now_ms)
  ms=$((end - start))
  # ops/sec = N / (ms/1000)
  if [ $ms -eq 0 ]; then
    iops=0
  else
    iops=$(awk "BEGIN{printf \"%.3f\", $N / ($ms/1000.0)}")
  fi
  write_csv "iops_create_1k" "$fsname" "$iops" "ops/s"

  # Delete N files
  start=$(now_ms)
  for i in $(seq 1 $N); do
    rm -f "$dir/f$i.txt"
  done
  end=$(now_ms)
  ms=$((end - start))
  if [ $ms -eq 0 ]; then
    iops=0
  else
    iops=$(awk "BEGIN{printf \"%.3f\", $N / ($ms/1000.0)}")
  fi
  write_csv "iops_delete_1k" "$fsname" "$iops" "ops/s"

  rmdir "$dir" || true
}

# Run tests for ext4 and fuse
log "Starting benchmark. Results -> $CSV"
# Option: uncomment to drop caches between big tests (requires root)
# drop_caches

do_latency "$EXT4_DIR" "ext4"
do_latency "$FUSE_MNT" "fuse"

# drop_caches
do_throughput "$EXT4_DIR" "ext4"
do_throughput "$FUSE_MNT" "fuse"

# drop_caches
do_iops "$EXT4_DIR" "ext4"
do_iops "$FUSE_MNT" "fuse"

log "Benchmarks finished. Results saved to $CSV"
log "Run ascii_graph.py to view ASCII charts:"
log "  python3 $BASEDIR/ascii_graph.py $CSV"
