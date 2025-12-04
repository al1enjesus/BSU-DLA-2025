# Базовые настройки компилятора
CC = gcc
CFLAGS = -Wall -g -D_FILE_OFFSET_BITS=64
LDFLAGS = 

# Автоматическое определение FUSE (fuse3 или fuse2)
FUSE3_LIBS = $(shell pkg-config fuse3 --libs 2>/dev/null)
FUSE3_CFLAGS = $(shell pkg-config fuse3 --cflags 2>/dev/null)

ifneq ($(FUSE3_LIBS),)
    # Используем fuse3 если найден
    FUSE_LIBS = $(FUSE3_LIBS)
    FUSE_CFLAGS = $(FUSE3_CFLAGS)
else
    # Пробуем fuse2
    FUSE2_LIBS = $(shell pkg-config fuse --libs 2>/dev/null)
    FUSE2_CFLAGS = $(shell pkg-config fuse --cflags 2>/dev/null)
    ifneq ($(FUSE2_LIBS),)
        FUSE_LIBS = $(FUSE2_LIBS)
        FUSE_CFLAGS = $(FUSE2_CFLAGS)
    else
        # Ручные настройки если pkg-config не сработал
        FUSE_LIBS = -lfuse3
        FUSE_CFLAGS = -I/usr/include/fuse3
    endif
endif

# Добавляем FUSE флаги к общим
CFLAGS += $(FUSE_CFLAGS)

# Цели сборки
TARGET_PASSTHROUGH = passthrough_fuse
TARGET_ARCHIVE = archive_fs
TARGET_MONITORING = monitoring_fs
TARGET_BENCHMARK = benchmark_latency

# Исходные файлы
SOURCES_PASSTHROUGH = passthrough_fuse.c
SOURCES_ARCHIVE = archive_fs.c
SOURCES_MONITORING = monitoring_fs.c
SOURCES_BENCHMARK = benchmark_latency.c

# Скрипты бенчмарков
BENCHMARK_SCRIPTS = benchmark_throughput.sh benchmark_iops.sh

# Библиотеки для каждого таргета
LIBS_PASSTHROUGH = $(FUSE_LIBS)
LIBS_ARCHIVE = $(FUSE_LIBS) -larchive
LIBS_MONITORING = $(FUSE_LIBS) -lpthread
LIBS_BENCHMARK = -lrt

# Основная цель - собирает всё
all: $(TARGET_PASSTHROUGH) $(TARGET_ARCHIVE) $(TARGET_MONITORING) benchmark

# Сборка passthrough файловой системы
$(TARGET_PASSTHROUGH): $(SOURCES_PASSTHROUGH)
	$(CC) $(CFLAGS) -o $(TARGET_PASSTHROUGH) $(SOURCES_PASSTHROUGH) $(LIBS_PASSTHROUGH)

# Сборка archive файловой системы  
$(TARGET_ARCHIVE): $(SOURCES_ARCHIVE)
	$(CC) $(CFLAGS) -o $(TARGET_ARCHIVE) $(SOURCES_ARCHIVE) $(LIBS_ARCHIVE)

# Сборка monitoring файловой системы
$(TARGET_MONITORING): $(SOURCES_MONITORING)
	$(CC) $(CFLAGS) -o $(TARGET_MONITORING) $(SOURCES_MONITORING) $(LIBS_MONITORING)

# Сборка бенчмарков
benchmark: $(TARGET_BENCHMARK)
	chmod +x $(BENCHMARK_SCRIPTS)

$(TARGET_BENCHMARK): $(SOURCES_BENCHMARK)
	$(CC) $(CFLAGS) -o $(TARGET_BENCHMARK) $(SOURCES_BENCHMARK) $(LIBS_BENCHMARK)

# Тестирование компиляции
test: all
	@echo "=== Testing compilation ==="
	@for target in $(TARGET_PASSTHROUGH) $(TARGET_ARCHIVE) $(TARGET_MONITORING); do \
		echo "Testing $$target..."; \
		./$$target --version 2>/dev/null || echo "$$target compiled successfully"; \
	done

# Запуск бенчмарков
run_benchmarks: benchmark
	@echo "=== Running FUSE Benchmarks ==="
	
	@# Создаем тестовые директории если их нет
	@mkdir -p /mnt/passthrough /mnt/monitoring /tmp/source
	
	@# Создаем тестовый файл если его нет
	@if [ ! -f /tmp/source/testfile ]; then \
		echo "Creating test file..."; \
		dd if=/dev/zero of=/tmp/source/testfile bs=1M count=10 status=none; \
	fi
	
	@echo "=== Testing Passthrough FUSE ==="
	@./benchmark_latency /mnt/passthrough testfile
	@./benchmark_throughput.sh /mnt/passthrough passthrough
	@./benchmark_iops.sh /mnt/passthrough passthrough
	
	@echo "=== Testing Monitoring FUSE ==="
	@./benchmark_latency /mnt/monitoring testfile
	@./benchmark_throughput.sh /mnt/monitoring monitoring
	@./benchmark_iops.sh /mnt/monitoring monitoring
	
	@echo "=== Testing ext4 (baseline) ==="
	@./benchmark_latency /tmp/source testfile
	@./benchmark_throughput.sh /tmp/source ext4
	@./benchmark_iops.sh /tmp/source ext4
	
	@echo "=== Benchmark results saved to CSV files ==="

# Построение графиков (требует Python с matplotlib)
plots:
	@echo "=== Generating plots ==="
	@if command -v python3 >/dev/null 2>&1; then \
		if python3 -c "import matplotlib" >/dev/null 2>&1; then \
			python3 plot_results.py; \
			echo "Plots generated successfully"; \
		else \
			echo "Python matplotlib not found. Install with: pip3 install matplotlib pandas"; \
		fi \
	else \
		echo "Python3 not found. Install Python3 to generate plots"; \
	fi

# Очистка
clean:
	rm -f $(TARGET_PASSTHROUGH) $(TARGET_ARCHIVE) $(TARGET_MONITORING) $(TARGET_BENCHMARK)

clean_benchmarks:
	rm -f $(TARGET_BENCHMARK) *.csv

clean_all: clean clean_benchmarks
	rm -f *.png

# Установка зависимостей (для Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libfuse3-dev fuse3 libarchive-dev pkg-config python3-matplotlib python3-pandas bc

# Информация о целях
help:
	@echo "Available targets:"
	@echo "  all              - Build all FUSE filesystems and benchmarks"
	@echo "  test             - Test compilation of all targets"
	@echo "  run_benchmarks   - Run all benchmarks"
	@echo "  plots            - Generate performance plots (requires Python matplotlib)"
	@echo "  clean            - Remove compiled binaries"
	@echo "  clean_benchmarks - Remove benchmark binaries and results"
	@echo "  clean_all        - Remove all generated files"
	@echo "  install-deps     - Install required dependencies (Ubuntu/Debian)"
	@echo ""
	@echo "Individual targets:"
	@echo "  $(TARGET_PASSTHROUGH) - Passthrough FUSE filesystem"
	@echo "  $(TARGET_ARCHIVE)     - Archive FUSE filesystem"
	@echo "  $(TARGET_MONITORING)  - Monitoring FUSE filesystem"
	@echo "  benchmark         - Build benchmark tools"

# Файлы для установки
.PHONY: all test run_benchmarks plots clean clean_benchmarks clean_all install-deps help benchmark
