CC = gcc
CFLAGS = -Wall -Wextra -O2

all: supervisor

supervisor: src/supervisor.c
	$(CC) $(CFLAGS) -o supervisor src/supervisor.c

clean:
	rm -f supervisor