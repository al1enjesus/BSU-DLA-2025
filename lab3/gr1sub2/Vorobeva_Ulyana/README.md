```bash
# Переход в папку
cd ~/BSU-DLA-2025/lab3/gr1sub2/Vorobeva_Ulyana

# Запуск анализа текущего процесса bash
./run.sh

# Запуск анализа конкретного PID
./run.sh <pid>

# Проверка с системными утилитами
ps -p <pid> -o pid,ppid,state,utime,stime,rss,nlwp,comm
top -b -n 1 -p <pid>
```