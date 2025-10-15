CC = gcc
CFLAGS = -std=gnu99 -O2 -Wall -Wextra
LDFLAGS =
PROGS = supervisor worker

all: $(PROGS)

supervisor: supervisor.c
	$(CC) $(CFLAGS) -o $@ supervisor.c $(LDFLAGS)

worker: worker.c
	$(CC) $(CFLAGS) -o $@ worker.c $(LDFLAGS)

clean:
	rm -f $(PROGS) *.o

.PHONY: all clean

