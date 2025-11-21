CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC
LDFLAGS = -ldl

TARGETS = libsyscall_spy.so benchmark simple_dynamic simple_static

.PHONY: all clean test

all: $(TARGETS)

libsyscall_spy.so: syscall_spy.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

benchmark: benchmark.c
	$(CC) $(CFLAGS) -o $@ $<

simple_dynamic: simple.c
	$(CC) $(CFLAGS) -o $@ $<

simple_static: simple.c
	$(CC) -static $(CFLAGS) -o $@ $<

install: all
	cp $(TARGETS) ../

clean:
	rm -f $(TARGETS) *.o

test: all
	@echo "=== Testing dynamic binary ==="
	@LD_PRELOAD=./libsyscall_spy.so ./simple_dynamic 2>&1 | head -5 || true
	@echo "=== Testing static binary ==="
	@LD_PRELOAD=./libsyscall_spy.so ./simple_static 2>&1 | head -2 || true