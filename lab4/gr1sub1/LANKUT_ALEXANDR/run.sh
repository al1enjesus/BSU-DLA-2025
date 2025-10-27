mkdir -p logs

echo "Testing ls..."
LD_PRELOAD=./build/libsyscall_spy.so ls > /dev/null 2> logs/ls_spy.log
strace -e trace=file,desc ls > /dev/null 2> logs/ls_strace.log

echo "Testing cat..."
echo "Test data" > logs/testfile.txt
LD_PRELOAD=./build/libsyscall_spy.so cat logs/testfile.txt > /dev/null 2> logs/cat_spy.log

echo "Testing grep..."
echo "needle" > logs/grep_test.txt
echo "haystack" >> logs/grep_test.txt
LD_PRELOAD=./build/libsyscall_spy.so grep needle logs/grep_test.txt > /dev/null 2> logs/grep_spy.log

echo "Running benchmark..."
./build/benchmark > logs/benchmark_open.log

echo "Running perf measurements..."
perf stat -e cycles,instructions,cache-misses,cache-references ./build/benchmark 2> logs/perf_stat.log

echo "Running cache experiment..."
echo "First run (cold cache):" > logs/cache_experiment.log
./build/benchmark >> logs/cache_experiment.log
echo -e "\nSecond run (warm cache):" >> logs/cache_experiment.log
./build/benchmark >> logs/cache_experiment.log

echo "Testing static binary..."
./build/static_test &> logs/static_test.log

echo "Creating summary..."
echo "System Call Analysis Summary" > logs/summary.txt
echo "==========================" >> logs/summary.txt
echo -e "\nls syscalls:" >> logs/summary.txt
grep -c "\[SPY\]" logs/ls_spy.log >> logs/summary.txt
echo -e "\ncat syscalls:" >> logs/summary.txt
grep -c "\[SPY\]" logs/cat_spy.log >> logs/summary.txt
echo -e "\ngrep syscalls:" >> logs/summary.txt
grep -c "\[SPY\]" logs/grep_spy.log >> logs/summary.txt
