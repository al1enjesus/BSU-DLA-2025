#!/bin/bash

# Демонстрация задания B: Планирование процессов
echo "=== Задание B: Планирование процессов (nice и CPU affinity) ==="

# Проверяем Python
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

# Проверяем необходимые утилиты
echo "Checking required tools..."
for tool in pidstat taskset renice ps; do
    if command -v $tool &> /dev/null; then
        echo "✓ $tool found"
    else
        echo "⚠ $tool not found (some features may be limited)"
    fi
done

echo ""
echo "=== Часть 1: Запуск процессов с разными настройками планирования ==="

CONFIG_FILE="config.json"

# Унифицированная конфигурация
cat > $CONFIG_FILE << 'EOF'
{
    "workers": 4,
    "mode_heavy": {"work_us": 500000, "sleep_us": 100000},
    "mode_light": {"work_us": 100000, "sleep_us": 400000},
    "scheduling": [
        {"worker_id": 0, "nice": 0, "cpu_affinity": [0]},
        {"worker_id": 1, "nice": 10, "cpu_affinity": [0]},
        {"worker_id": 2, "nice": 0, "cpu_affinity": [1]},
        {"worker_id": 3, "nice": -5, "cpu_affinity": [1]}
    ]
}
EOF

echo "Configuration created:"
python3 -c "
import json
with open('$CONFIG_FILE', 'r') as f:
    config = json.load(f)
print('Workers:', config['workers'])
for sched in config['scheduling']:
    print(f'  Worker {sched[\"worker_id\"]}: nice={sched[\"nice\"]}, affinity={sched[\"cpu_affinity\"]}')
"

# Запуск супервизора
echo ""
echo "Starting supervisor..."
cd src
python3 supervisor.py ../$CONFIG_FILE &
SUPERVISOR_PID=$!
cd ..

sleep 3

# Получаем PID воркеров
WORKER_PIDS=()
for i in {0..3}; do
    PID=$(pgrep -f "worker.py $i" | head -1)
    if [ -n "$PID" ]; then
        WORKER_PIDS[i]=$PID
        echo "Worker $i PID: $PID"
    fi
done

echo ""
echo "=== Часть 2: Сбор метрик с помощью pidstat ==="
pidstat -u -p $(echo ${WORKER_PIDS[@]} | tr ' ' ',') 1 10 > pidstat_output.txt &
sleep 11

echo ""
echo "=== Часть 3: Анализ влияния nice на распределение CPU ==="
for i in {0..3}; do
    if [ -n "${WORKER_PIDS[i]}" ]; then
        echo ""
        echo "Worker $i (PID: ${WORKER_PIDS[i]}):"
        grep "^.*${WORKER_PIDS[i]}.*" pidstat_output.txt | tail -5 | awk '{print "  Time:", $1, "CPU:", $7 "%", "User:", $8 "%", "System:", $9 "%"}'
        NICE=$(ps -o nice= -p ${WORKER_PIDS[i]} 2>/dev/null || echo "?")
        echo "  Current nice: $NICE"
    fi
done

echo ""
echo "=== Часть 4: Проверка CPU affinity ==="
if command -v taskset &> /dev/null; then
    for i in {0..3}; do
        if [ -n "${WORKER_PIDS[i]}" ]; then
            AFFINITY=$(taskset -cp ${WORKER_PIDS[i]} 2>/dev/null | cut -d: -f2 | tr -d ' ')
            echo "Worker $i affinity: $AFFINITY"
        fi
    done
else
    echo "taskset not available - skipping affinity check"
fi

echo ""
echo "=== Часть 5: Демонстрация изменения nice в реальном времени ==="
if [ -n "${WORKER_PIDS[0]}" ] && [ -n "${WORKER_PIDS[1]}" ]; then
    echo "Changing nice values for Workers 0 and 1..."
    for i in 0 1; do
        NICE_VAL=$(( i == 0 ? 5 : 15 ))
        if command -v renice &> /dev/null; then
            sudo renice -n $NICE_VAL -p ${WORKER_PIDS[i]} 2>/dev/null || renice -n $NICE_VAL -p ${WORKER_PIDS[i]}
        else
            echo "Warning: renice not available, skipping nice change for Worker $i"
        fi
    done

    echo "New nice values:"
    for i in 0 1; do
        echo "Worker $i: $(ps -o nice= -p ${WORKER_PIDS[i]} 2>/dev/null || echo '?')"
    done

    echo "Collecting metrics after nice change..."
    pidstat -u -p ${WORKER_PIDS[0]},${WORKER_PIDS[1]} 1 5 > pidstat_after.txt
    grep "^.*${WORKER_PIDS[0]}.*" pidstat_after.txt | tail -3 | awk '{print "  Worker 0 - CPU:", $7 "%"}'
    grep "^.*${WORKER_PIDS[1]}.*" pidstat_after.txt | tail -3 | awk '{print "  Worker 1 - CPU:", $7 "%"}'
fi

echo ""
echo "=== Часть 6: Завершение демонстрации ==="
echo "Stopping supervisor and workers..."
kill -TERM $SUPERVISOR_PID
sleep 3

rm -f pidstat_output.txt pidstat_after.txt

echo ""
echo "=== Демонстрация завершена ==="
echo ""
echo "Выводы:"
echo "1. Процессы с меньшим nice (высшим приоритетом) получают больше CPU времени"
echo "2. CPU affinity ограничивает выполнение процессов определенными ядрами"
echo "3. На Linux эффект особенно заметен при конкуренции за ресурсы"
echo "4. На macOS nice работает, но CPU affinity не поддерживается"
