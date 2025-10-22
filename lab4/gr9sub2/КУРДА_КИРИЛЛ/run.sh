#!/bin/bash
set -euo pipefail

# run.sh — запуск экспериментов из корня проекта
# предполагает структуру:
#  ./src/libsyscall_spy.so
#  ./src/benchmark
#  ./logs/   (будет создан)

ROOT="$(cd "$(dirname "$0")"; pwd)"
SRC_DIR="$ROOT/src"
LOG_DIR="$ROOT/logs"

mkdir -p "$LOG_DIR"

echo "[run.sh] Source dir: $SRC_DIR"
echo "[run.sh] Logs dir:   $LOG_DIR"

# 1) curl (полный скач)
echo "[run.sh] Running curl (with LD_PRELOAD) ..."
LD_PRELOAD="$SRC_DIR/libsyscall_spy.so" \
  curl -L -o /tmp/lab4_curl_out https://example.com \
  > "$LOG_DIR/curl_stdout.log" 2> "$LOG_DIR/curl_ldpreload.log" || true

# 2) wget (полный скач)
echo "[run.sh] Running wget (with LD_PRELOAD) ..."
LD_PRELOAD="$SRC_DIR/libsyscall_spy.so" \
  wget -O /tmp/lab4_wget_out https://example.com \
  > "$LOG_DIR/wget_stdout.log" 2> "$LOG_DIR/wget_ldpreload.log" || true

# 3) benchmark
echo "[run.sh] Running benchmark (stdout -> log) ..."
cd "$SRC_DIR"
./benchmark > "$LOG_DIR/benchmark_output.log" 2>&1 || true

# 4) perf stat (may require sudo)
echo "[run.sh] Running perf stat (may require elevated privileges) ..."
perf stat -e cycles,instructions,context-switches,page-faults ./benchmark 2> "$LOG_DIR/perf_benchmark.log" || true

# 5) build static hello (may fail on WSL) and test LD_PRELOAD (expect no output)
echo "[run.sh] Building hello_static ..."
cat > "$SRC_DIR/hello.c" <<'HEL'
#include <stdio.h>
int main(void){ puts("hello static"); return 0; }
HEL
gcc -static -o "$SRC_DIR/hello_static" "$SRC_DIR/hello.c" 2> "$LOG_DIR/hello_static_build.log" || true
echo "[run.sh] Running hello_static with LD_PRELOAD (should not be intercepted) ..."
LD_PRELOAD="$SRC_DIR/libsyscall_spy.so" "$SRC_DIR/hello_static" 2> "$LOG_DIR/hello_static.log" || true

echo "[run.sh] Done. Logs: $LOG_DIR"
