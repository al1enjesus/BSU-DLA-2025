#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
MYFUSE=$ROOT/myfuse
OUTDIR=$ROOT/tools/results
mkdir -p $OUTDIR


# ITER can be overridden from environment for quick runs, default 200
ITER=${ITER:-200}
SMALL=4096

latency_csv=$OUTDIR/latency.csv
throughput_csv=$OUTDIR/throughput.csv
iops_csv=$OUTDIR/iops.csv

echo "mode,fs,operation,iter_no,time_sec" > $latency_csv
echo "mode,fs,size_mb,secs,mb_per_s" > $throughput_csv
echo "mode,fs,count,secs,files_per_s" > $iops_csv

prepare_dirs() {
  mkdir -p /tmp/source
  # ensure test files
  dd if=/dev/zero of=/tmp/source/test_4k bs=4096 count=1 >/dev/null 2>&1 || true
  dd if=/dev/zero of=/tmp/source/big_1m bs=1M count=1 >/dev/null 2>&1 || true
  dd if=/dev/zero of=/tmp/source/big_10m bs=1M count=10 >/dev/null 2>&1 || true
  dd if=/dev/zero of=/tmp/source/big_100m bs=1M count=100 >/dev/null 2>&1 || true
}


mount_tmpfs() {
  # tmpfs mount requires root. If not running as root, skip tmpfs tests.
  if [ "$(id -u)" -ne 0 ]; then
    echo "Skipping tmpfs mount: not running as root"
    return 1
  fi
  mkdir -p /tmp/tmpfs_source
  mount -t tmpfs -o size=512M tmpfs /tmp/tmpfs_source
  # copy data
  cp /tmp/source/* /tmp/tmpfs_source/ || true
  return 0
}

umount_if_mounted() {
  if mount | grep -q " $1 "; then
    fusermount3 -u $1 2>/dev/null || fusermount -u $1 2>/dev/null || umount $1 2>/dev/null || true
  fi
}

mount_fuse_mode() {
  mode=$1
  mountpoint=$2
  mkdir -p $mountpoint
  # ensure project log directory exists and logfile is writable to avoid permission issues
  LOGDIR=$ROOT/log
  mkdir -p "$LOGDIR"
  logfile="$LOGDIR/fuse_${mountpoint##*/}.log"
  touch "$logfile" 2>/dev/null || true
  chmod 666 "$logfile" 2>/dev/null || true
  case $mode in
    passthrough)
      $MYFUSE /tmp/source $mountpoint -d 2> "$logfile" &
      ;;
    rot13)
      $MYFUSE /tmp/source $mountpoint -m rot13 -d 2> "$logfile" &
      ;;
    uppercase)
      $MYFUSE /tmp/source $mountpoint -m uppercase -d 2> "$logfile" &
      ;;
  esac
  sleep 0.5
}

stop_background_myfuse() {
  pkill -f "$MYFUSE" || true
  sleep 0.2
}

run_latency_on() {
  mode=$1; fsname=$2; path=$3
  file=$path/test_4k
  for op in open read write getattr; do
    for i in $(seq 1 $ITER); do
      case $op in
        open)
          t=$(python3 $HERE/measure.py --operation open --file $file --iterations 1)
          ;;
        read)
          t=$(python3 $HERE/measure.py --operation read --file $file --iterations 1 --size $SMALL)
          ;;
        write)
          # ensure writable file
          # create test write file if missing
          touch $path/test_write || true
          t=$(python3 $HERE/measure.py --operation write --file $path/test_write --iterations 1 --size $SMALL)
          ;;
        getattr)
          t=$(python3 $HERE/measure.py --operation getattr --file $file --iterations 1)
          ;;
      esac
      echo "$mode,$fsname,$op,${i},$t" >> $latency_csv
    done
  done
}

run_throughput_on() {
  mode=$1; fsname=$2; path=$3
  for sz in 1 10 100; do
    f=$path/big_${sz}m
    if [ ! -f $f ]; then
      dd if=/dev/zero of=$f bs=1M count=$sz >/dev/null 2>&1 || true
    fi
    out=$(python3 $HERE/throughput.py $f)
    # prints mb,secs,mb/s
    IFS=',' read mb secs mbs <<< "$out"
    echo "$mode,$fsname,$mb,$secs,$mbs" >> $throughput_csv
  done
}

run_iops_on() {
  mode=$1; fsname=$2; path=$3
  for count in 100 500 1000; do
    d=$path/iops_test
    rm -rf $d || true
    mkdir -p $d
    t0=$(date +%s.%N)
    for i in $(seq 1 $count); do
      dd if=/dev/zero of=$d/file$i bs=1K count=1 >/dev/null 2>&1
    done
    t1=$(date +%s.%N)
    secs=$(echo "$t1 - $t0" | bc -l)
    fps=$(echo "$count / $secs" | bc -l)
    echo "$mode,$fsname,$count,$secs,$fps" >> $iops_csv
    rm -rf $d || true
  done
}

# prepare
prepare_dirs

if mount_tmpfs; then
  TMPFS_MOUNTED=1
else
  TMPFS_MOUNTED=0
fi

echo "Running native (source) tests..."
run_latency_on native source /tmp/source
run_throughput_on native source /tmp/source
run_iops_on native source /tmp/source

if [ "$TMPFS_MOUNTED" -eq 1 ]; then
  echo "Running tmpfs tests..."
  run_latency_on tmpfs tmpfs /tmp/tmpfs_source
  run_throughput_on tmpfs tmpfs /tmp/tmpfs_source
  run_iops_on tmpfs tmpfs /tmp/tmpfs_source
else
  echo "Skipping tmpfs tests (not mounted)"
fi

echo "Running FUSE modes..."
for entry in "${targets_fuse[@]}"; do
  IFS=':' read mode:mp <<< "$entry" || true
done

# mount and test each FUSE mode
umount_if_mounted /tmp/mnt_fuse || true
umount_if_mounted /tmp/mnt_fuse_rot13 || true
umount_if_mounted /tmp/mnt_fuse_upper || true


# FUSE modes: passthrough, rot13, uppercase
for mode in passthrough rot13 uppercase; do
  case "$mode" in
    passthrough)
      mp=/tmp/mnt_fuse
      ;;
    rot13)
      mp=/tmp/mnt_fuse_rot13
      ;;
    uppercase)
      mp=/tmp/mnt_fuse_upper
      ;;
  esac
  umount_if_mounted "$mp" || true
  mount_fuse_mode $mode $mp
  # wait a bit for mount
  sleep 0.5
  run_latency_on fuse_$mode fuse $mp
  run_throughput_on fuse_$mode fuse $mp
  run_iops_on fuse_$mode fuse $mp
  stop_background_myfuse
done

echo "Benchmarks complete. Results in $OUTDIR. Now generating plots..."
if python3 -c "import matplotlib" >/dev/null 2>&1; then
  python3 $HERE/process_and_plot.py $latency_csv $throughput_csv $iops_csv
  echo "Plots saved in the current directory: latency_summary.png, throughput.png, iops.png"
else
  echo "matplotlib not found — skipping plotting. CSV results available in $OUTDIR"
fi
