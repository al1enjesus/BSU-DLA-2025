#!/bin/bash
# Упрощенный системный мониторинг

echo "=================================================="
echo "          СИСТЕМНЫЙ МОНИТОРИНГ ПАМЯТИ"
echo "=================================================="

echo " Запустите в другом терминале:"
echo "   ./scripts/taskD/memory.sh"
echo "   ./scripts/taskD/signals.sh"
echo "   ./scripts/taskD/limits.sh"
echo ""
echo " Наблюдаем за системой..."
echo "Для выхода: Ctrl+C"
echo ""

while true; do
    clear
    echo "=== СИСТЕМНЫЙ МОНИТОРИНГ ($(date +%H:%M:%S)) ==="
    echo ""
    
    # Процессы mem_touch
    echo "--- Процессы mem_touch ---"
    ps aux | head -1  # заголовок
    ps aux | grep -E "[.]/mem_touch" | grep -v grep
    
    echo ""
    echo "--- Использование памяти ---"
    free -h | grep -E "(total|Mem:)"
    
    echo ""
    echo "--- Ключевые метрики ---"
    grep -E "(MemAvailable|SwapFree)" /proc/meminfo | while read line; do
        echo "  $line"
    done
    
    sleep 2
done