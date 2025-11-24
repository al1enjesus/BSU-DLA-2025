#!/bin/bash

CONFIG_FILE="config.ini"
SUP_BIN="./supervisor"

echo "--- 1. Сборка проекта ---"
make || { echo "Сборка не удалась. Выходим."; exit 1; }

# -----------------------------------------------------------
# --- ЧАСТЬ A: Graceful Shutdown/Reload (Базовый тест) ---
# -----------------------------------------------------------

echo "--- 2. Запуск супервизора для теста A ---"

# Создаем конфиг для теста A (3 воркера)
cat > $CONFIG_FILE <<EOL
workers=3
work_heavy_us=9000
sleep_heavy_us=1000
work_light_us=2000
sleep_light_us=8000
nice_default=0      
nice_low_prio=10    
affinity_cpu0=0     
affinity_cpu1=1     
EOL

$SUP_BIN $CONFIG_FILE &
SUPERVISOR_PID=$!
echo "Супервизор запущен с PID: $SUPERVISOR_PID"

echo "--- 3. Ожидание запуска воркеров и переключение режимов ---"
sleep 4 
ps -o pid,ppid,comm | grep -E "$SUPERVISOR_PID|worker"

echo "--- 4. Переключение в ЛЕГКИЙ режим (SIGUSR1) ---"
kill -SIGUSR1 $SUPERVISOR_PID
sleep 5

echo "--- 5. Демонстрация Graceful Reload (SIGHUP) ---"
# Изменим конфиг: 3 воркера -> 2 воркера, и сменим параметры
cat > $CONFIG_FILE <<EOL
workers=2
work_heavy_us=10000
sleep_heavy_us=500
work_light_us=1000
sleep_light_us=9000
nice_default=0      
nice_low_prio=10    
affinity_cpu0=0     
affinity_cpu1=1     
EOL

kill -SIGHUP $SUPERVISOR_PID
sleep 5
echo "Текущее дерево процессов после RELOAD (должно быть 2 новых воркера):"
ps -o pid,ppid,comm | grep -E "$SUPERVISOR_PID|worker"

echo "--- 6. Завершение теста A ---"
kill -SIGTERM $SUPERVISOR_PID
wait $SUPERVISOR_PID
echo "Супервизор завершился."

# -----------------------------------------------------------
# --- ЧАСТЬ B: Планирование (nice и CPU-аффинити) ---
# -----------------------------------------------------------

echo ""
echo "--- 7. Эксперимент B: nice и CPU-аффинити ---"

# 7.1. Подготовка: Настройка конфига для 4 воркеров
cat > $CONFIG_FILE <<EOL
workers=4
work_heavy_us=9000
sleep_heavy_us=1000
work_light_us=2000
sleep_light_us=8000
nice_default=0      # W1, W2
nice_low_prio=10    # W3, W4
affinity_cpu0=0     # W1, W3 (Конкуренция на CPU 0)
affinity_cpu1=1     # W2, W4 (Конкуренция на CPU 1)
EOL

$SUP_BIN $CONFIG_FILE &
SUPERVISOR_PID=$!
echo "Супервизор запущен с PID: $SUPERVISOR_PID"
sleep 4 # Ждем запуска всех 4-х воркеров

# Находим PID воркеров
WORKER_PIDS=$(pgrep -P $SUPERVISOR_PID worker)
echo "Woker PIDs: $WORKER_PIDS"

echo ""
echo "--- 7.2. Проверка NICE и CPU-аффинити через ps (PSR=CPU) ---"
# pid=ID, ni=nice, comm=имя, psr=cpu на котором исполняется, cgroup
ps -o pid,nice,comm,psr | grep -E "$WORKER_PIDS|PID"

echo ""
echo "--- 7.3. Снятие метрик pidstat (10 секунд) ---"
echo "Ожидаемо: W1/W2 (nice=0) получат больше CPU, чем W3/W4 (nice=10)."
# Запускаем pidstat на 10 секунд с интервалом 1 секунда, фильтруя по воркерам.
pidstat -u -p $WORKER_PIDS 1 10 

echo ""
echo "--- 7.4. Очистка ---"
kill -SIGTERM $SUPERVISOR_PID
wait $SUPERVISOR_PID
echo "Супервизор завершен."
echo "--- Эксперимент B завершен ---"