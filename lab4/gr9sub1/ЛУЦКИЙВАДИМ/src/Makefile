CC = gcc
CFLAGS = -shared -fPIC -O2
LDFLAGS = -ldl

all: libsyscall_spy.so benchmark

libsyscall_spy.so: syscall_spy.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo " Библиотека скомпилирована: $@"

benchmark: benchmark.c
	$(CC) -O2 -o $@ $<
	@echo " Бенчмарк скомпилирован: $@"

clean:
	rm -f libsyscall_spy.so benchmark
	@echo " Очистка завершена"

test-quick: all
	@echo " Быстрая проверка..."
	@LD_PRELOAD=./libsyscall_spy.so ls /tmp > /dev/null 2>&1 && echo " LD_PRELOAD работает" || echo " LD_PRELOAD не работает"
	@./benchmark > /dev/null 2>&1 && echo " Бенчмарк работает" || echo " Бенчмарк не работает"

.PHONY: all clean test-quick