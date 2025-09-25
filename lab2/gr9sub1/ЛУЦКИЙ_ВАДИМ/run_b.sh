#!/bin/bash

echo "=== Лабораторная работа 2 - Задание B ==="
echo "Демонстрация управления планированием (nice и CPU affinity)"

# Переходим в папку src
cd src

echo ""
echo "1. Проверяем систему..."
echo "Количество CPU ядер: $(nproc)"
echo "Текущие настройки nice:"
ps -eo pid,ni,cmd | grep -E "(python|supervisor)" | grep -v grep || echo "Процессы не найдены"

echo ""
echo "2. Запускаем супервизор с планированием..."
python3 supervisor_b.py config.json &

# Запоминаем PID супервизора
SUPERVISOR_PID=$!
echo "Супервизор запущен с PID: $SUPERVISOR_PID"

# Даём время на запуск воркеров
echo ""
echo "3. Ждём запуска воркеров (5 секунд)..."
sleep 5

echo ""
echo "4. Проверяем настройки планирования воркеров:"
echo "   Команда: ps -eo pid,ni,psr,cmd | grep worker"
ps -eo pid,ni,psr,cmd | grep worker | grep -v grep

echo ""
echo "5. Запускаем мониторинг CPU использования (10 секунд)..."
echo "   Команда: pidstat -u 1 10"
pidstat -u 1 10 > pidstat_output.log &

echo ""
echo "6. Демонстрация работы воркеров (15 секунд)..."
sleep 15

echo ""
echo "7. Тестируем переключение режимов:"
echo "   Переключение в лёгкий режим (SIGUSR1)..."
kill -USR1 $SUPERVISOR_PID
sleep 5

echo "   Переключение в тяжёлый режим (SIGUSR2)..."
kill -USR2 $SUPERVISOR_PID
sleep 5

echo ""
echo "8. Анализ результатов мониторинга:"
echo "   Результаты pidstat:"
cat pidstat_output.log

echo ""
echo "9. Сравниваем распределение CPU для процессов с разным nice:"
if command -v python3 >/dev/null 2>&1; then
    python3 - << EOF
import re
with open('pidstat_output.log', 'r') as f:
    lines = f.readlines()

# Ищем строки с worker процессами
worker_stats = {}
for line in lines:
    if 'worker.py' in line:
        parts = re.split(r'\s+', line.strip())
        if len(parts) >= 8:
            pid = parts[0]
            cpu_usage = parts[7]
            if pid.isdigit() and cpu_usage.replace('.', '').isdigit():
                worker_stats[pid] = float(cpu_usage)

print("Статистика использования CPU по процессам:")
for pid, cpu in worker_stats.items():
    print(f"  PID {pid}: {cpu}% CPU")

# Получаем nice значения для этих PID
import subprocess
result = subprocess.run(['ps', '-eo', 'pid,ni,cmd'], capture_output=True, text=True)
for line in result.stdout.split('\n'):
    for pid in worker_stats.keys():
        if pid in line and 'worker.py' in line:
            parts = line.strip().split()
            if len(parts) >= 3:
                nice_val = parts[1]
                print(f"  PID {pid}: nice={nice_val}, CPU usage={worker_stats[pid]}%")
EOF
fi

echo ""
echo "10. Graceful shutdown (SIGTERM)..."
kill -TERM $SUPERVISOR_PID

# Ждём завершения
sleep 3

echo ""
echo "11. Финальная проверка процессов:"
ps aux | grep -E "(supervisor|worker).py" | grep -v grep && echo "Есть незавершённые процессы" || echo "Все процессы завершены"

echo ""
echo "=== Демонстрация задания B завершена ==="

# Очистка
rm -f pidstat_output.log