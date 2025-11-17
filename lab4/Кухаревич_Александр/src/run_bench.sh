#!/bin/bash
set -euo pipefail

BENCH=./benchmark
LIB=./libsyscall_spy.so

if [ ! -f "$BENCH" ]; then
  echo "Building benchmark..."
  make bench
fi

echo "=== Running benchmark WITHOUT LD_PRELOAD ==="
$BENCH 2>&1 | tee benchmark_no_spy.out

echo
echo "=== Running benchmark WITH LD_PRELOAD ($LIB) ==="
if [ ! -f "$LIB" ]; then
  echo "Library $LIB not found. Build it with: make spy"
  exit 1
fi

LD_PRELOAD=$LIB $BENCH 2>&1 | tee benchmark_with_spy.out

echo
echo "=== Attempt perf stat (if available) on one run WITHOUT spy ==="
if command -v perf >/dev/null 2>&1; then
  perf stat -e cycles,instructions,context-switches,page-faults -r 3 $BENCH 2>&1 | tee perf_no_spy.out
else
  echo "perf not found - skip perf_no_spy"
fi

echo
echo "=== Attempt perf stat (if available) on one run WITH spy ==="
if command -v perf >/dev/null 2>&1; then
  perf stat -e cycles,instructions,context-switches,page-faults -r 3 LD_PRELOAD=$LIB $BENCH 2>&1 | tee perf_with_spy.out
else
  echo "perf not found - skip perf_with_spy"
fi

echo
echo "Results saved to:"
echo " - benchmark_no_spy.out"
echo " - benchmark_with_spy.out"
echo " - perf_no_spy.out (if perf present)"
echo " - perf_with_spy.out (if perf present)"

