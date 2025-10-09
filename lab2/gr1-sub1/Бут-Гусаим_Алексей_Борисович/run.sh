echo "=== Быстрая демонстрация ==="
# Задача A
echo "1. Запуск задачи A..."
python3 src/supervisor_A.py config_A.json &
PID_A=$!
sleep 3
kill -USR1 $PID_A
sleep 2
kill -TERM $PID_A
wait $PID_A
echo ""
# Задача B  
echo "2. Запуск задачи B..."
python3 src/supervisor_B.py config_B.json &
PID_B=$!
sleep 3
ps -o pid,ni,psr,pcpu -p $PID_B $(ps --ppid $PID_B -o pid=)
sleep 2
kill -TERM $PID_B
wait $PID_B