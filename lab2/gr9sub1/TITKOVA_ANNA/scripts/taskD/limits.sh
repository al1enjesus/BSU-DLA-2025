#!/bin/bash
# Демонстрация ограничений памяти

echo "=================================================="
echo "              ОГРАНИЧЕНИЕ ПАМЯТИ"
echo "=================================================="

BIN_DIR="bin"
LOG_DIR="logs"
mkdir -p "$LOG_DIR"

cd "$BIN_DIR"

echo "Текущие лимиты памяти:"
ulimit -a | grep "virtual memory"

echo ""
echo "=== ТЕСТ 1: Без ограничений ==="
echo "Запуск с целью 128MB..."
./mem_touch --rss-mb 128 --step-mb 32 --sleep-ms 300 > "../$LOG_DIR/no_limit.log" 2>&1 &
PID1=$!
echo "PID: $PID1"

# Мониторим 8 секунд
for i in {1..8}; do
    if ps -p $PID1 > /dev/null 2>&1; then
        RSS_KB=$(ps -p $PID1 -o rss --no-headers 2>/dev/null | awk '{print $1}')
        if [ -n "$RSS_KB" ]; then
            RSS_MB=$((RSS_KB / 1024))
            printf "  Шаг %d: RSS=%dMB\n" $i $RSS_MB
        fi
    else
        echo "  Процесс завершился"
        break
    fi
    sleep 1
done
kill -TERM $PID1 2>/dev/null

echo ""
echo "Лог теста 1:"
tail -3 "../$LOG_DIR/no_limit.log"

echo ""
echo "=== ТЕСТ 2: С ограничением 80MB ==="  
echo "Запуск с --limit-as-mb 80..."
./mem_touch --rss-mb 128 --step-mb 32 --sleep-ms 300 --limit-as-mb 80 > "../$LOG_DIR/with_limit.log" 2>&1 &
PID2=$!
echo "PID: $PID2"

echo "Мониторим процесс..."
for i in {1..10}; do
    if ps -p $PID2 > /dev/null 2>&1; then
        RSS_KB=$(ps -p $PID2 -o rss --no-headers 2>/dev/null | awk '{print $1}')
        if [ -n "$RSS_KB" ]; then
            RSS_MB=$((RSS_KB / 1024))
            printf "  Шаг %d: RSS=%dMB\n" $i $RSS_MB
        fi
        
        # Проверяем лог на ошибки malloc
        if tail -1 "../$LOG_DIR/with_limit.log" 2>/dev/null | grep -q "malloc"; then
            echo "  ОБНАРУЖЕНА ОШИБКА MALLOC!"
            break
        fi
    else
        echo "  Процесс завершился на шаге $i"
        break
    fi
    sleep 1
done

echo ""
echo "Лог теста 2:"
tail -5 "../$LOG_DIR/with_limit.log"

echo ""
# Проверяем результаты
if grep -q "malloc" "../$LOG_DIR/with_limit.log"; then
    echo ""
    echo "LIMIT_AS РАБОТАЕТ КОРРЕКТНО!"
else
    echo ""
    echo "Лимит не сработал как ожидалось"
fi

kill -TERM $PID2 2>/dev/null