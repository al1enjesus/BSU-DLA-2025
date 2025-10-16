CC=gcc
CFLAGS=-O2 -fPIC -Wall -Wextra -g
LDFLAGS=-ldl -shared

all: libsyscall_spy.so benchmark

libsyscall_spy.so: syscall_spy.c
	$(CC) $(CFLAGS) -o libsyscall_spy.so syscall_spy.c $(LDFLAGS)

benchmark: benchmark.c
	$(CC) -O2 -march=native -o benchmark benchmark.c

clean:
	rm -f libsyscall_spy.so benchmark hello_static

.PHONY: all clean
