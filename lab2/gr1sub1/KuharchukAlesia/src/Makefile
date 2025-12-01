CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS = supervisor worker

all: $(TARGETS)

supervisor: supervisor.c config.c
  $(CC) $(CFLAGS) -o supervisor supervisor.c config.c

worker: worker.c config.c
  $(CC) $(CFLAGS) -o worker worker.c config.c

clean:
  rm -f $(TARGETS)

.PHONY: all clean
