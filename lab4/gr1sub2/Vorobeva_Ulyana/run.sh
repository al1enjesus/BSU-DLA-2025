#!/usr/bin/env bash
set -euo pipefail

OUT=build
LIB="$OUT/libsyscall_spy.so"
LOGDIR=logs
mkdir -p $LOGDIR

# Programs for analysis (для номера 4 -> ls, cat, grep)
echo "Programs: ls, cat, grep" > $LOGDIR/summary.txt

# 1) Build
make all

# 2) Run spy on ls
echo "=== LD_PRELOAD ls ===" > $LOGDIR/ls_spy.log
LD_PRELOAD=./$LIB ls -la / |& tee -a $LOGDIR/ls_spy.log

# 3) Run spy on cat
echo "=== LD_PRELOAD cat ===" > $LOGDIR/cat_spy.log
echo "Hello world" > $LOGDIR/testfile.txt
LD_PRELOAD=./$LIB cat $LOGDIR/testfile.txt |& tee -a $LOGDIR/cat_spy.log

# 4) Run spy on grep
echo "=== LD_PRELOAD grep ===" > $LOGDIR/grep_spy.log
echo -e "one\ntwo\nthree\nhello\n" > $LOGDIR/grep_test.txt
LD_PRELOAD=./$LIB grep hello $LOGDIR/grep_test.txt |& tee -a $LOGDIR/grep_spy.log

# 5) Benchmark (10000 iterations)
echo "=== Benchmark open+close & write ===" > $LOGDIR/benchmark_open.log
$OUT/benchmark 10000 | tee -a $LOGDIR/benchmark_open.log

# 6) Static test
echo "=== Static test with LD_PRELOAD ===" > $LOGDIR/static_test.log
LD_PRELOAD=./$LIB $OUT/static_test |& tee -a $LOGDIR/static_test.log || true

echo "Done. Logs in $LOGDIR/"
