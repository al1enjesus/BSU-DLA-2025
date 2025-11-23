#!/bin/bash
set -e

BASE_DIR="$(dirname "$0")"
LOG_DIR="$BASE_DIR/../../logs/B"
LOG_FILE="$LOG_DIR/benchmark_log.txt"

make -C "$BASE_DIR" all
mkdir -p "$LOG_DIR"

"$BASE_DIR/benchmark" | tee "$LOG_FILE"
