#!/usr/bin/env bash
# Simple benchmark script to measure read/write times and collect CSV
set -e
OUT=bench_results.csv
echo "test,fs,bytes,time_sec" > $OUT
SRC=/tmp/source
MOUNT=/mnt/fuse
mkdir -p $SRC $MOUNT
# prepare files
dd if=/dev/zero of=$SRC/bigfile bs=1M count=100 >/dev/null 2>&1 || true
# read from fuse
for bs in 4096 65536 1048576; do
  for cnt in 100 1000; do
    echo -n "read_fuse_${bs}_${cnt},fuse," >> $OUT
    /usr/bin/time -f "%e" dd if=$MOUNT/bigfile bs=$bs count=$cnt of=/dev/null 2>&1 | tail -n1 | awk '{print $1","$2}'
  done
done

# note: this is a minimal script — expand as needed
