# Makefile for Lab 4 - System Calls
# Variant 19: Programs gcc, make, as
# Location: ~/BSU-DLA-2025/lab4/gr9sub1/Polyakov_Andrei/src/

CC = gcc
CFLAGS = -Wall -O2 -fPIC -Wno-nonnull-compare
LDFLAGS = -ldl

# Targets
all: libsyscall_spy.so benchmark test_programs

# Task A: LD_PRELOAD library
libsyscall_spy.so: syscall_spy.c
	$(CC) -shared $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built LD_PRELOAD library: $@"

# Task B: Benchmark
benchmark: benchmark.c
	$(CC) $(CFLAGS) -o $@ $< -lm
	@echo "Built benchmark program: $@"

# Test programs for experiments
test_programs: hello hello_static

hello: hello.c
	$(CC) -o $@ $<
	@echo "Built dynamic test program: $@"

hello_static: hello.c
	$(CC) -static -o $@ $<
	@echo "Built static test program: $@"

hello.c:
	@echo '#include <stdio.h>' > $@
	@echo 'int main() { printf("Hello, World!\\n"); return 0; }' >> $@

# Clean build artifacts
clean:
	rm -f *.so *.o benchmark hello hello_static hello.c
	rm -f test.s test.o hello.o test_prog test_prog.o testprog
	rm -f Makefile.test

.PHONY: all clean test_programs
