CC = gcc
CFLAGS = -Wall -Wextra

all: build/libsyscall_spy.so build/benchmark build/static_test

build/libsyscall_spy.so: src/syscall_spy.c
	$(CC) -shared -fPIC -o $@ $< -ldl -lpthread

build/benchmark: src/benchmark.c
	$(CC) $(CFLAGS) -o $@ $< -lrt

build/static_test: src/static_test.c
	$(CC) $(CFLAGS) -static -o $@ $<

clean:
	rm -f build/* logs/*

.PHONY: all clean