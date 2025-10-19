#!/bin/bash

echo "=== Демонстрация задания B: Планирование ==="
echo ""

# Собираем проект
echo "1. Сборка проекта..."
cd src
make clean
make
cd ..

echo ""
echo "2. Запуск супервизора с 3 workers..."
cd src
./supervisor &
SUPER_PID=$!
cd ..

sleep 3
echo ""
echo "3. Мониторинг процессов с pidstat (10 секунд)..."
echo "Запустите в отдельном терминале: pidstat -u 1 10"
echo "Или нажмите Enter для продолжения..."
read

echo ""
echo "4. Проверка nice значений и CPU affinity..."
echo "Processes info:"
ps -o pid,ni,psr,comm -p $(pgrep worker) 2>/dev/null

echo ""
echo "5. Workers info (из их вывода):"
echo "Посмотрите вывод супервизора для информации о CPU и nice"

echo ""
echo "6. Завершение..."
kill -TERM $SUPER_PID
wait $SUPER_PID 2>/dev/null

echo ""
echo "=== Демонстрация завершена ==="
echo "Для полного анализа запустите в отдельном терминале:"
echo "pidstat -u 1 30"
