#!/bin/bash

set -euo pipefail
DIR=$(cd "$(dirname "$0")"; pwd)
cd "$DIR"
mkdir -p logs

echo "Building..."
cd src
make clean
make all
cd ..

LIB_PATH="$DIR/src/libsyscall_spy.so"

echo "Running LD_PRELOAD on find..."
LD_PRELOAD="$LIB_PATH" find /tmp -maxdepth 2 -name "*.log" 2> logs/find_ldpreload.log || true

echo "Running LD_PRELOAD on tar..."
LD_PRELOAD="$LIB_PATH" tar -cf /tmp/lab4_test.tar /tmp/lab4_test 2> logs/tar_ldpreload.log || true

echo "Running LD_PRELOAD on cp..."
touch /tmp/lab4_srcfile && LD_PRELOAD="$LIB_PATH" cp /tmp/lab4_srcfile /tmp/lab4_dstfile 2> logs/cp_ldpreload.log || true

echo "Running benchmark..."
./src/benchmark > logs/benchmark_output.log 2>&1
perf stat -e cycles,instructions,context-switches,page-faults ./src/benchmark 2> logs/perf_benchmark.log || true

gcc -static -o src/hello_static src/hello.c
LD_PRELOAD="$LIB_PATH" ./src/hello_static 2> logs/hello_static.log || true

echo "Done. See logs/"
