CC=gcc

all: supervisor worker

supervisor: supervisor.c
	$(CC) supervisor.c -o supervisor

worker: worker.c
	$(CC) worker.c -o worker

clean:
	rm -f supervisor worker
