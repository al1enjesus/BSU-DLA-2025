CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_DEFAULT_SOURCE
TARGETS = supervisor worker

all: $(TARGETS)

supervisor: supervisor.c
	$(CC) $(CFLAGS) -o supervisor supervisor.c

worker: worker.c
	$(CC) $(CFLAGS) -o worker worker.c -lm

clean:
	rm -f $(TARGETS)

.PHONY: all clean