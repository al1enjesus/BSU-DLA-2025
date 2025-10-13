CC=gcc
CFLAGS=-fPIC -Wall -Werror
LDFLAGS_SPY=-shared -ldl

.PHONY: all clean run_ls run_cat run_grep

all: libsyscall_spy.so hello_static

libsyscall_spy.so: src/syscall_spy.c
	mkdir -p lib
	$(CC) $(CFLAGS) $(LDFLAGS_SPY) -o lib/$@ $< 

hello_static: src/hello.c
	mkdir -p bin
	$(CC) -static -o bin/$@ $<

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
