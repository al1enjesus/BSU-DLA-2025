make -C ../../samples

../../samples/mem_touch --rss-mb 128 --step-mb 64 --sleep-ms 500 &
PID=$!

for i in {1..10}; do
    cat /proc/$PID/status | grep -E "VmRSS|VmSize"
    sleep 1
done

kill $PID