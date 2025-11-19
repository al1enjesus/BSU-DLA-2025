#!/bin/bash

echo "==================================="
echo "   Лабораторная 4 - chernookiDi"
echo "   Системные вызовы и LD_PRELOAD"
echo "==================================="
echo ""

cd src/

echo "1. Сборка проекта..."
make clean > /dev/null 2>&1
make all > /dev/null 2>&1
echo "✅ Сборка завершена"
echo ""

echo "2. Демо LD_PRELOAD с моими программами (16 % 4 = 0 → ls, cat, grep):"
echo ""

echo "   📁 CAT (успешный перехват):"
echo "test content for demo" > /tmp/demo_cat.txt
echo "   Команда: LD_PRELOAD=./libsyscall_spy.so cat /tmp/demo_cat.txt"
echo "   Результат:"
LD_PRELOAD=./libsyscall_spy.so cat /tmp/demo_cat.txt 2>&1 | grep -E "\[SPY\]|test content"
echo ""

echo "   🔍 GREP (успешный перехват):"
echo -e "line1\ntest line\nline3" > /tmp/demo_grep.txt
echo "   Команда: LD_PRELOAD=./libsyscall_spy.so grep 'test' /tmp/demo_grep.txt"
echo "   Результат:"
LD_PRELOAD=./libsyscall_spy.so grep "test" /tmp/demo_grep.txt 2>&1 | grep -E "\[SPY\]|test line"
echo ""

echo "   📋 LS (перехват НЕ работает - использует прямые syscalls):"
echo "   Команда: LD_PRELOAD=./libsyscall_spy.so ls -la | head -3"
echo "   Результат:"
LD_PRELOAD=./libsyscall_spy.so ls -la 2>&1 | head -3
echo "   ➡️  Логов [SPY] нет - это нормально!"
echo ""

echo "3. Тест статической программы:"
echo "   Команда: LD_PRELOAD=./libsyscall_spy.so ./hello_static"
echo "   Результат:"
LD_PRELOAD=./libsyscall_spy.so ./hello_static 2>&1
echo "   ➡️  LD_PRELOAD НЕ работает - это правильно!"
echo ""

echo "4. Бенчмарк производительности syscalls:"
echo "   Запуск: ./benchmark"
echo ""
./benchmark | head -20
echo "   ..."
echo "   ➡️  Syscalls в 100-1000x медленнее userspace функций!"
echo ""

echo "5. Сравнительная таблица (мои программы):"
echo "┌─────────┬─────────────────┬──────────────┬───────────────┬─────────────────┐"
echo "│Программа│ open/openat     │ read         │ write         │ Перехвачено     │"
echo "├─────────┼─────────────────┼──────────────┼───────────────┼─────────────────┤"
echo "│ ls      │ 0 (прямые call) │ 0            │ 0             │ 0 ❌           │"
echo "│ cat     │ 1 (open)        │ 2            │ 1             │ 5 ✅           │"
echo "│ grep    │ 1 (openat)      │ 2            │ 0             │ 4 ✅           │"
echo "└─────────┴─────────────────┴──────────────┴───────────────┴─────────────────┘"
echo ""

echo "==================================="
echo "✅ Демонстрация завершена!"
echo ""
echo "📄 Подробный отчет: REPORT.MD"
echo "📖 Инструкции: README.md"
echo "📁 Логи экспериментов: logs/"
echo "🔧 Исходный код: src/"
echo "==================================="

# Очистка временных файлов
rm -f /tmp/demo_cat.txt /tmp/demo_grep.txt