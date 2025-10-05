CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -ldl

# Цели сборки
.PHONY: all clean test

all: libsyscall_spy.so benchmark mytracer hello_static

# Библиотека для LD_PRELOAD
libsyscall_spy.so: syscall_spy.c
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $< $(LDFLAGS)

# Бенчмарк системных вызовов
benchmark: benchmark.c
	$(CC) $(CFLAGS) -o $@ $<

# Упрощённый strace
mytracer: mytracer.c syscall_names.h
	$(CC) $(CFLAGS) -o $@ mytracer.c

# Статическая программа для тестирования LD_PRELOAD
hello_static: hello_static.c
	$(CC) -static -o $@ $<

# Тестирование
test: all
	@echo "=== Тест 1: LD_PRELOAD с динамической программой ==="
	LD_PRELOAD=./libsyscall_spy.so /bin/echo "Hello from echo" 2>&1 | head -10
	
	@echo -e "\n=== Тест 2: LD_PRELOAD со статической программой ==="
	LD_PRELOAD=./libsyscall_spy.so ./hello_static 2>&1 | head -5
	
	@echo -e "\n=== Тест 3: Бенчмарк (первые строки) ==="
	./benchmark | head -20
	
	@echo -e "\n=== Тест 4: Tracer на простой программе ==="
	./mytracer /bin/true | head -15

# Проверка зависимостей библиотек
check-deps:
	@echo "=== Проверка динамических зависимостей ==="
	ldd /bin/echo | head -5
	@echo -e "\n=== Проверка статической программы ==="
	ldd ./hello_static 2>&1 | head -3 || echo "Статическая программа не имеет зависимостей"

# Эксперименты для отчёта
experiments: all
	@echo "=== Запуск экспериментов для отчёта ==="
	@mkdir -p ../logs
	
	@echo "1. Тестирование LD_PRELOAD на gcc..."
	LD_PRELOAD=./libsyscall_spy.so gcc --version > ../logs/gcc_test.log 2>&1
	
	@echo "2. Тестирование LD_PRELOAD на make..."
	LD_PRELOAD=./libsyscall_spy.so make --version > ../logs/make_test.log 2>&1
	
	@echo "3. Запуск бенчмарка..."
	./benchmark > ../logs/benchmark_results.log 2>&1
	
	@echo "4. Тестирование tracer..."
	./mytracer /bin/true > ../logs/tracer_true.log 2>&1
	
	@echo "Логи сохранены в ../logs/"

# Очистка
clean:
	rm -f libsyscall_spy.so benchmark mytracer hello_static
	rm -f ../logs/*.log

help:
	@echo "Доступные цели:"
	@echo "  all          - собрать все программы"
	@echo "  test         - быстрые тесты"
	@echo "  experiments  - полные эксперименты для отчёта"
	@echo "  check-deps   - проверить зависимости библиотек"
	@echo "  clean        - удалить собранные файлы"
	@echo "  help         - показать эту справку"