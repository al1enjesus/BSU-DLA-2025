CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -ldl

# Основные цели
all: libsyscall_spy.so benchmark hello_static

# Библиотека для LD_PRELOAD
libsyscall_spy.so: syscall_spy.c
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $< $(LDFLAGS)

# Программа бенчмарка
benchmark: benchmark.c
	$(CC) $(CFLAGS) -o $@ $<

# Простая программа для тестов
hello.c:
	echo '#include <stdio.h>' > hello.c
	echo 'int main() { printf("Hello World\\n"); return 0; }' >> hello.c

# Статически слинкованная программа для тестирования LD_PRELOAD
hello_static: hello.c
	$(CC) -static -o $@ $<

# Очистка
clean:
	rm -f libsyscall_spy.so benchmark hello_static hello.c *.o

# Тесты
test-preload: libsyscall_spy.so
	@echo "=== Тест LD_PRELOAD с ls ==="
	LD_PRELOAD=./libsyscall_spy.so ls -la | head -20
	@echo ""
	@echo "=== Тест с cat ==="
	echo "test content" > /tmp/test_file
	LD_PRELOAD=./libsyscall_spy.so cat /tmp/test_file 2>&1 | head -10
	rm -f /tmp/test_file
	@echo ""
	@echo "=== Тест с grep ==="
	echo -e "line1\\ntest line\\nline3" | LD_PRELOAD=./libsyscall_spy.so grep "test" 2>&1 | head -10

test-static: hello_static libsyscall_spy.so
	@echo "=== Тест на статической программе (должен НЕ работать) ==="
	LD_PRELOAD=./libsyscall_spy.so ./hello_static 2>&1 || true

test-benchmark: benchmark
	@echo "=== Запуск бенчмарка ==="
	./benchmark

# Полный тест
test: test-preload test-static test-benchmark

.PHONY: all clean test test-preload test-static test-benchmark