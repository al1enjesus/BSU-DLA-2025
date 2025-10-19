echo "=== Быстрое сравнение утилит ==="
PID=$$
echo -e "\n1. PS:"
ps -o pid,ppid,state,thcount,time,vsz,rss,comm -p $PID
echo -e "\n2. PIDSTAT:"
pidstat -p $PID 1 1
echo -e "\n3. TOP:"
top -b -n 1 -p $PID | head -20
echo -e "\nГотово!"