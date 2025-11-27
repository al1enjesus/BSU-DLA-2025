#!/bin/bash

# Упрощенный скрипт тестирования для лабораторной работы 4

echo "=== Лабораторная 4: Системные вызовы ==="
echo "Номер в списке: 15 (программы: gcc, make, as)"
echo "==========================================="

# Создаём папку для логов
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
LOG_DIR="logs/experiment_${TIMESTAMP}"
mkdir -p "${LOG_DIR}"

echo "Логи будут сохраняться в: ${LOG_DIR}"
echo ""

# Простая функция логирования
log() {
    echo "$(date +'%H:%M:%S') $1" | tee -a "${LOG_DIR}/experiment.log"
}

# Сначала создаем папку логов, потом начинаем логировать
log "Начало экспериментов"

echo -e "\n🔧 КОМПИЛЯЦИЯ"
cd src

log "Компиляция библиотеки и бенчмарка..."
make clean > /dev/null 2>&1
make > "../${LOG_DIR}/compile.log" 2>&1

if [ ! -f "libsyscall_spy.so" ] || [ ! -f "benchmark" ]; then
    echo "❌ Ошибка компиляции! Проверьте ${LOG_DIR}/compile.log"
    cat "../${LOG_DIR}/compile.log"
    exit 1
fi

log "✅ Компиляция успешна"
cd ..

echo -e "\n📊 ЗАДАНИЕ A: LD_PRELOAD ПЕРЕХВАТ"

# Создаём тестовые файлы
echo "Создание тестовых файлов..."
cat > test.s << 'EOF'
.global _start
_start:
    mov $60, %rax
    mov $0, %rdi
    syscall
EOF

cat > test_makefile << 'EOF'
all:
	@echo 'Hello from make'
EOF

cat > hello.c << 'EOF'
#include <stdio.h>
int main() { printf("Hello static\n"); return 0; }
EOF

# Компилируем статическую программу
gcc -static -o hello_static hello.c > /dev/null 2>&1 || echo "⚠️  Предупреждение: не удалось скомпилировать статическую программу"

# Тестируем программы
echo -e "\n🔍 Тестирование gcc"
LD_PRELOAD=./src/libsyscall_spy.so gcc --version > "${LOG_DIR}/gcc_output.log" 2> "${LOG_DIR}/gcc_syscalls.log"
echo "✅ gcc протестирован"

echo -e "\n🔍 Тестирование make"
LD_PRELOAD=./src/libsyscall_spy.so make -f test_makefile > "${LOG_DIR}/make_output.log" 2> "${LOG_DIR}/make_syscalls.log"
echo "✅ make протестирован"

echo -e "\n🔍 Тестирование as (ассемблер)"
LD_PRELOAD=./src/libsyscall_spy.so as test.s -o test.o > "${LOG_DIR}/as_output.log" 2> "${LOG_DIR}/as_syscalls.log"
echo "✅ as протестирован"

echo -e "\n🔍 Тестирование статической программы"
LD_PRELOAD=./src/libsyscall_spy.so ./hello_static > "${LOG_DIR}/static_output.log" 2>&1
echo "✅ статическая программа протестирована"

echo -e "\n⏱️  ЗАДАНИЕ B: BENCHMARK СИСТЕМНЫХ ВЫЗОВОВ"

echo -e "\n🔍 Запуск бенчмарка"
./src/benchmark > "${LOG_DIR}/benchmark.log" 2>&1
echo "✅ бенчмарк выполнен"

echo -e "\n🔍 Измерение производительности через perf"
perf stat -e cycles,instructions,context-switches,page-faults ./src/benchmark > /dev/null 2> "${LOG_DIR}/perf_stat.log"
echo "✅ perf stat выполнен"

echo -e "\n🔍 Эксперимент с кэшем"
echo "=== Холодный кэш ===" > "${LOG_DIR}/cache_experiment.log"
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null 2>&1 || true
{ time (LD_PRELOAD=./src/libsyscall_spy.so cat /etc/hosts > /dev/null 2>&1); } 2>> "${LOG_DIR}/cache_experiment.log"

echo "=== Горячий кэш ===" >> "${LOG_DIR}/cache_experiment.log" 
{ time (LD_PRELOAD=./src/libsyscall_spy.so cat /etc/hosts > /dev/null 2>&1); } 2>> "${LOG_DIR}/cache_experiment.log"
echo "✅ эксперимент с кэшем выполнен"

echo -e "\n📈 АНАЛИЗ РЕЗУЛЬТАТОВ"

# Собираем статистику
echo "=== СТАТИСТИКА ===" > "${LOG_DIR}/summary.txt"

echo -e "\nКоличество системных вызовов по программам:" >> "${LOG_DIR}/summary.txt"
echo "gcc:   $(grep -c "\[SPY\]" "${LOG_DIR}/gcc_syscalls.log" 2>/dev/null || echo "0")" >> "${LOG_DIR}/summary.txt"
echo "make:  $(grep -c "\[SPY\]" "${LOG_DIR}/make_syscalls.log" 2>/dev/null || echo "0")" >> "${LOG_DIR}/summary.txt" 
echo "as:    $(grep -c "\[SPY\]" "${LOG_DIR}/as_syscalls.log" 2>/dev/null || echo "0")" >> "${LOG_DIR}/summary.txt"

echo -e "\nТипы системных вызовов (gcc):" >> "${LOG_DIR}/summary.txt"
grep "\[SPY\]" "${LOG_DIR}/gcc_syscalls.log" 2>/dev/null | cut -d' ' -f2 | sort | uniq -c | sort -nr >> "${LOG_DIR}/summary.txt" 2>/dev/null || echo "Нет данных" >> "${LOG_DIR}/summary.txt"

echo -e "\nРезультаты бенчмарка:" >> "${LOG_DIR}/summary.txt"
grep -A2 "Benchmarking:" "${LOG_DIR}/benchmark.log" >> "${LOG_DIR}/summary.txt" 2>/dev/null || echo "Нет данных бенчмарка" >> "${LOG_DIR}/summary.txt"

# Показываем краткую статистику
echo -e "\n📋 КРАТКИЕ РЕЗУЛЬТАТЫ:"
cat "${LOG_DIR}/summary.txt"

# Очистка временных файлов
echo -e "\n🧹 ОЧИСТКА"
rm -f test.s test_makefile hello.c hello hello_static test.o

echo -e "\n✅ ВСЕ ЭКСПЕРИМЕНТЫ ЗАВЕРШЕНЫ!"
echo ""
echo "📁 Логи сохранены в: ${LOG_DIR}"
echo ""
echo "Файлы в директории логов:"
ls -la "${LOG_DIR}/"

echo -e "\nДля просмотра подробных результатов:"
echo "  tail -50 ${LOG_DIR}/gcc_syscalls.log    # Системные вызовы gcc"
echo "  cat ${LOG_DIR}/benchmark.log            # Результаты бенчмарка"
echo "  cat ${LOG_DIR}/summary.txt              # Сводка результатов"