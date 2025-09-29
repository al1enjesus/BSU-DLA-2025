make -C /root/BSU-DLA-2025/lab2/samples

# Старт двух процессов с nice = 0 и nice = 10
nice -n 0 /root/BSU-DLA-2025/lab2/samples/cpu_burn --work-us 9000 --sleep-us 1000 --duration 10 &
PID1=$!
nice -n 10 /root/BSU-DLA-2025/lab2/samples/cpu_burn --work-us 9000 --sleep-us 1000 --duration 10 &
PID2=$!

# Ожидание завершения процессов
wait $PID1 $PID2

# Анализ использования CPU с помощью pidstat
pidstat -u -p $PID1,$PID2 1 10