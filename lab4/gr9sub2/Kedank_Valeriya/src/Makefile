CC=gcc
CFLAGS=-Wall -fPIC -O2 -g
LDFLAGS=-shared -ldl

all: libsyscall_spy.so

libsyscall_spy.so: syscall_spy.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f libsyscall_spy.so *.o
