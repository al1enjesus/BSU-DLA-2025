mkdir -p logs

echo "System Information:" > logs/perf_stat_detailed.log
echo "==================" >> logs/perf_stat_detailed.log
cat /proc/cpuinfo | grep "model name" | head -n1 >> logs/perf_stat_detailed.log
cat /proc/meminfo | grep MemTotal >> logs/perf_stat_detailed.log

echo -e "\nCache Information:" >> logs/perf_stat_detailed.log
echo "=================" >> logs/perf_stat_detailed.log
cat /sys/devices/system/cpu/cpu0/cache/index*/size 2>/dev/null >> logs/perf_stat_detailed.log

echo -e "\nScheduler Statistics:" >> logs/perf_stat_detailed.log
echo "====================" >> logs/perf_stat_detailed.log
cat /proc/schedstat >> logs/perf_stat_detailed.log

echo -e "\nVMStat Information:" >> logs/perf_stat_detailed.log
echo "==================" >> logs/perf_stat_detailed.log
vmstat 1 5 >> logs/perf_stat_detailed.log

echo -e "\nBenchmark Execution:" >> logs/perf_stat_detailed.log
echo "===================" >> logs/perf_stat_detailed.log
/usr/bin/time -v ./build/benchmark 2>> logs/perf_stat_detailed.log