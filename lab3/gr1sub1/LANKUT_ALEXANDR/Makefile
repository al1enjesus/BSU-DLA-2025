CC = gcc
CFLAGS = -Wall -Wextra -O2

all: pstat

pstat: src/pstat.c
	$(CC) $(CFLAGS) -o pstat src/pstat.c

clean:
	rm -f pstat

.PHONY: all clean