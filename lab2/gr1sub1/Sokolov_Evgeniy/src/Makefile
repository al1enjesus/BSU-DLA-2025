CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = 

TARGETS = supervisor worker pstat

.PHONY: all clean

all: $(TARGETS)

supervisor: supervisor.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

worker: worker.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

pstat: pstat.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)