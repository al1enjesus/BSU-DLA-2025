#!/bin/bash
# Скрипт для воспроизведения экспериментов из задания E*

set -e
cd "$(dirname "$0")"

echo "=== Эксперименты с диагностикой и профилированием ==="
echo

# Проверяем доступность утилит
check_tool() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "ВНИМАНИЕ: утилита $1 не найдена, пропускаем соответствующий эксперимент"
        return 1
    fi
    return 0
}

echo "1. Сборка проекта..."
make -C src/ clean >/dev/null
make -C src/ >/dev/null

echo "2. Эксперимент с CPU нагрузкой и nice..."
if check_tool taskset && check_tool pidstat; then
    echo "   Запускаем два cpu_burn процесса на одном ядре..."
    cd ../samples
    taskset -c 0 ./cpu_burn --duration 10 &
    PID1=$!
    taskset -c 0 ./cpu_burn --duration 10 &
    PID2=$!
    
    sleep 2
    echo "   До изменения nice:"
    pidstat -u 1 2 -p $PID1,$PID2 | grep cpu_burn || true
    
    echo "   Изменяем nice для первого процесса..."
    renice +10 $PID1 >/dev/null
    
    sleep 2
    echo "   После изменения nice:"
    pidstat -u 1 2 -p $PID1,$PID2 | grep cpu_burn || true
    
    wait $PID1 $PID2
    cd ../gr1sub1/Sokolov_Evgeniy
    echo "   ✓ CPU эксперимент завершён"
else
    echo "   ✗ Пропущен (нет taskset или pidstat)"
fi

echo
echo "3. Эксперимент с памятью..."
echo "   Запускаем mem_touch..."
cd ../samples
./mem_touch --rss-mb 50 --step-mb 10 --sleep-ms 2000 &
MEM_PID=$!

sleep 3
if [ -d "/proc/$MEM_PID" ]; then
    echo "   Анализ процесса mem_touch (PID=$MEM_PID):"
    cd ../gr1sub1/Sokolov_Evgeniy
    src/pstat $MEM_PID | head -10
    
    echo "   Добавляем память (SIGUSR1)..."
    kill -USR1 $MEM_PID
    sleep 3
    
    echo "   Состояние после увеличения памяти:"
    src/pstat $MEM_PID | grep -E "(VmRSS|RssAnon)" || true
    
    echo "   Уменьшаем память (SIGUSR2)..."
    kill -USR2 $MEM_PID
    sleep 3
    
    echo "   Завершаем процесс..."
    kill -TERM $MEM_PID
    wait $MEM_PID 2>/dev/null || true
    echo "   ✓ Память эксперимент завершён"
else
    echo "   ✗ Процесс mem_touch завершился слишком быстро"
fi
cd ../gr1sub1/Sokolov_Evgeniy

echo
echo "4. Эксперимент с супервизором..."
echo "   Запускаем супервизор на 10 секунд..."
cd src/
./supervisor ../config.ini &
SUPERVISOR_PID=$!
cd ..

sleep 2
WORKER_PIDS=$(pgrep -P $SUPERVISOR_PID) || true
if [ -n "$WORKER_PIDS" ]; then
    echo "   Воркеры: $WORKER_PIDS"
    
    echo "   Переключение в лёгкий режим..."
    kill -USR1 $SUPERVISOR_PID
    sleep 2
    
    echo "   Переключение в тяжёлый режим..."
    kill -USR2 $SUPERVISOR_PID
    sleep 2
    
    echo "   Анализ одного воркера:"
    WORKER_PID=$(echo $WORKER_PIDS | cut -d' ' -f1)
    src/pstat $WORKER_PID | head -8
    
    echo "   Graceful shutdown..."
    kill -TERM $SUPERVISOR_PID
    wait $SUPERVISOR_PID 2>/dev/null || true
    echo "   ✓ Супервизор эксперимент завершён"
else
    echo "   ✗ Воркеры не найдены"
    kill $SUPERVISOR_PID 2>/dev/null || true
fi

echo
echo "=== Все эксперименты завершены ==="
echo "Для дополнительного анализа используйте:"
echo "  strace -c -p <PID>     # анализ системных вызовов"
echo "  perf stat -p <PID>     # аппаратные счётчики"
echo "  pidstat -u 1 5 -p <PID>  # мониторинг CPU"