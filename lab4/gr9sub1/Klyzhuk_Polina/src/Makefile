CC = gcc
CFLAGS = -O2 -fPIC -Wall -Wextra
LDFLAGS = -ldl

.PHONY: all clean

all: libsyscall_spy.so benchmark

libsyscall_spy.so: syscall_spy.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

benchmark: benchmark.c
	$(CC) -O2 -march=native -o $@ benchmark.c

clean:
	rm -f libsyscall_spy.so benchmark
