# pstat — Linux /proc

### Сборка
cd src && make

### Запуск
./pstat <pid>

### Проверка и сравнение
ps -p <pid> -o pid,ppid,state,time,rss,vsz,comm
top -p <pid> -b -n 1
cat /proc/<pid>/status
cat /proc/<pid>/io
cat /proc/<pid>/smaps_rollup
