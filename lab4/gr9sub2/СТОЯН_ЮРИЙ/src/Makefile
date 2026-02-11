# Makefile для Лабораторной работы 4
# Вариант: find, tar, cp

CC = gcc
CFLAGS = -O2 -Wall -Wextra
LDFLAGS = -ldl -lm

# Цели по умолчанию
.PHONY: all clean test perf_report

# Основные цели
all: libsyscall_spy.so benchmark benchmark_cold

# ============================================================================
# ЗАДАНИЕ A: LD_PRELOAD библиотека
# ============================================================================

# Основная библиотека для перехвата syscalls
libsyscall_spy.so: syscall_spy.c
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $< $(LDFLAGS)

# Тестовая программа для LD_PRELOAD
test_program: test_program.c
	$(CC) $(CFLAGS) -o $@ $<

# Статически скомпилированная программа для теста
static_test: static_test.c
	$(CC) $(CFLAGS) -static -o $@ $<

# ============================================================================
# ЗАДАНИЕ B: Benchmark системных вызовов
# ============================================================================

# Основной бенчмарк
benchmark: benchmark.c
	$(CC) $(CFLAGS) -o $@ $<

# Бенчмарк с холодным кэшем
benchmark_cold: benchmark_cold.c
	$(CC) $(CFLAGS) -o $@ $<

# Бенчмарк для perf stat анализа
benchmark_perf: benchmark_perf.c
	$(CC) $(CFLAGS) -o $@ $<

# Бенчмарк файловых операций
benchmark_open: benchmark_open.c
	$(CC) $(CFLAGS) -o $@ $<

# ============================================================================
# ТЕСТИРОВАНИЕ
# ============================================================================

# Тест задания A - перехват системных вызовов
test_a: libsyscall_spy.so
	@echo "=== ТЕСТ ЗАДАНИЯ A: LD_PRELOAD ==="
	@echo "Тестируем перехват на программе ls:"
	@LD_PRELOAD=./libsyscall_spy.so ls /tmp 2>&1 | head -10
	@echo ""
	@echo "Тестируем перехват на find:"
	@mkdir -p test_data
	@echo "test" > test_data/file1.txt
	@LD_PRELOAD=./libsyscall_spy.so find test_data -name "*.txt" 2>&1 | head -10
	@echo ""

# Тест задания B - бенчмарки
test_b: benchmark benchmark_cold
	@echo "=== ТЕСТ ЗАДАНИЯ B: BENCHMARK ==="
	@echo "Запуск основного бенчмарка:"
	@./benchmark
	@echo ""
	@echo "Запуск бенчмарка с холодным кэшем:"
	@./benchmark_cold

# Полный тест всех заданий
test: test_a test_b
	@echo "=== ВСЕ ТЕСТЫ ЗАВЕРШЕНЫ ==="

# ============================================================================
# PERF STAT АНАЛИЗ
# ============================================================================

# Анализ производительности через perf stat
perf_report: benchmark_perf benchmark_open
	@echo "=== PERF STAT АНАЛИЗ ==="
	@if command -v perf >/dev/null 2>&1; then \
		echo "1. Анализ benchmark_perf:"; \
		perf stat -e cycles,instructions,context-switches,page-faults ./benchmark_perf 2>&1 | tail -8; \
		echo ""; \
		echo "2. Анализ benchmark_open:"; \
		perf stat -e cycles,instructions,context-switches,page-faults ./benchmark_open 2>&1 | tail -8; \
	else \
		echo "perf не установлен. Установите: sudo apt install linux-tools-common"; \
	fi

# ============================================================================
# СБОР ДАННЫХ ДЛЯ ОТЧЕТА
# ============================================================================

# Сбор всех данных для отчета
report_data: all
	@echo "=== СБОР ДАННЫХ ДЛЯ ОТЧЕТА ==="
	@mkdir -p logs
	@echo "1. Сбор логов LD_PRELOAD..."
	@mkdir -p test_report
	@echo "test data" > test_report/file.txt
	@LD_PRELOAD=./libsyscall_spy.so find test_report -type f > logs/ld_preload_find.log 2>&1
	@LD_PRELOAD=./libsyscall_spy.so cp test_report/file.txt test_report/copy.txt > logs/ld_preload_cp.log 2>&1
	@echo "2. Запуск бенчмарков..."
	@./benchmark > logs/benchmark_hot.log 2>&1
	@./benchmark_cold > logs/benchmark_cold.log 2>&1
	@echo "3. Данные собраны в папке logs/"

# ============================================================================
# ВСПОМОГАТЕЛЬНЫЕ ЦЕЛИ
# ============================================================================

# Создание тестовых файлов
setup_test:
	@echo "Создание тестовой структуры..."
	@mkdir -p test_data/{folder1,subdir}
	@echo "File 1 content" > test_data/file1.txt
	@echo "File 2 content" > test_data/file2.txt
	@echo "File in folder1" > test_data/folder1/file3.txt
	@echo "File in subdir" > test_data/subdir/file4.txt
	@echo "Тестовая структура создана"

# Очистка
clean:
	rm -f *.so benchmark benchmark_cold benchmark_perf benchmark_open test_program static_test
	rm -rf test_data test_report logs
	rm -f /tmp/test_file_* /tmp/bench_* /tmp/perf_test_* /tmp/benchmark_test_*

# Справка
help:
	@echo "Доступные цели:"
	@echo "  all          - компиляция всех программ"
	@echo "  test_a       - тест задания A (LD_PRELOAD)"
	@echo "  test_b       - тест задания B (Benchmark)"
	@echo "  test         - полный тест всех заданий"
	@echo "  perf_report  - анализ производительности через perf stat"
	@echo "  report_data  - сбор данных для отчета"
	@echo "  setup_test   - создание тестовой структуры"
	@echo "  clean        - очистка"
	@echo ""
	@echo "Примеры использования:"
	@echo "  make test_a                    # Тест перехвата syscalls"
	@echo "  LD_PRELOAD=./libsyscall_spy.so ls /tmp  # Ручной тест"
	@echo "  make perf_report              # Анализ производительности"

# ============================================================================
# ФАЙЛЫ ДЛЯ КОМПИЛЯЦИИ
# ============================================================================

# Зависимости
syscall_spy_simple.c: syscall_spy.c
	cp syscall_spy.c syscall_spy_simple.c

static_test.c:
	@echo "Создание static_test.c..."
	@cat > static_test.c << 'EOF'
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("static_test.txt", O_CREAT | O_WRONLY, 0644);
    write(fd, "Hello from static program!", 26);
    close(fd);
    printf("Static program completed\n");
    return 0;
}
EOF

test_program.c:
	@echo "Создание test_program.c..."
	@cat > test_program.c << 'EOF'
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    printf("Test program started\\n");
    
    int fd = open("test.txt", O_CREAT | O_WRONLY, 0644);
    write(fd, "Test data", 9);
    close(fd);
    
    printf("Test program completed\\n");
    return 0;
}
EOF

# Информация о системе
info:
	@echo "=== ИНФОРМАЦИЯ О СИСТЕМЕ ==="
	@echo "Компилятор: $(CC)"
	@echo "Версия: $(shell $(CC) --version | head -1)"
	@echo "ОС: $(shell uname -s -r)"
	@echo "CPU: $(shell grep "model name" /proc/cpuinfo | head -1 | cut -d: -f2)"
	@echo "Кэш: $(shell lscpu | grep "cache:" | head -3)"