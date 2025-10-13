CC=gcc
CFLAGS=-fPIC -Wall -Werror
LDFLAGS_SPY=-shared -ldl

.PHONY: all clean run_ls run_cat run_grep run_benchmark run_static_test perf_benchmark

all: libsyscall_spy.so hello_static benchmark mytracer

libsyscall_spy.so: src/syscall_spy.c
	mkdir -p lib
	$(CC) $(CFLAGS) $(LDFLAGS_SPY) -o lib/$@ $< 

hello_static: src/hello.c
	mkdir -p bin
	$(CC) -static -o bin/$@ $<

benchmark: src/benchmark.c
	mkdir -p bin
	$(CC) -o bin/$@ $<

benchmark_open: src/benchmark_open.c
	mkdir -p bin
	$(CC) -o bin/$@ $<

mytracer: src/mytracer.c src/syscall_names.h
	mkdir -p bin
	$(CC) -o bin/$@ $(filter %.c,$^)

run_benchmark:
	mkdir -p logs
	./bin/benchmark > logs/benchmark.log

perf_benchmark:
	mkdir -p logs
	perf stat -e cycles,instructions,context-switches,page-faults ./bin/benchmark_open > logs/perf_benchmark.log 2>&1

run_ls:
	mkdir -p logs
	LD_PRELOAD=./lib/libsyscall_spy.so ls -la > logs/ls.log 2>&1

run_cat:
	mkdir -p logs
	LD_PRELOAD=./lib/libsyscall_spy.so cat /etc/passwd > logs/cat.log 2>&1

run_grep:
	mkdir -p logs
	LD_PRELOAD=./lib/libsyscall_spy.so grep root /etc/passwd > logs/grep.log 2>&1

run_static_test:
	mkdir -p logs
	LD_PRELOAD=./lib/libsyscall_spy.so ./bin/hello_static > logs/static.log 2>&1

clean:
	rm -rf lib bin logs