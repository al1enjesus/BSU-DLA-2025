CC=gcc
CFLAGS=-Wall -O2

all: supervisor worker

supervisor: supervisor.c
	$(CC) $(CFLAGS) supervisor.c -o supervisor

worker: worker.c
	$(CC) $(CFLAGS) worker.c -o worker

clean:
	rm -f supervisor worker
