CC=gcc
CFLAGS=-O2 -fPIC -Wall -Wextra
LDFLAGS=-ldl
SRC=src
BUILD=.

all: libsyscall_spy.so benchmark hello_static open_bench

libsyscall_spy.so: $(SRC)/syscall_spy.c
	$(CC) $(CFLAGS) -shared -fPIC -o $@ $^ $(LDFLAGS)

benchmark: $(SRC)/benchmark.c
	$(CC) -O2 -march=native -o $@ $^ 2> /dev/null

hello_static: $(SRC)/hello.c
	# статическая линковка
	$(CC) -static -O2 -o $@ $^

open_bench: $(SRC)/open_bench.c
	$(CC) -O2 -march=native -o $@ $^ 2> /dev/null

clean:
	rm -f libsyscall_spy.so benchmark hello_static

.PHONY: all clean