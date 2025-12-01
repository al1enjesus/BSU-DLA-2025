CC = gcc
CFLAGS = -std=gnu11 -O2 -Wall -Wextra
LDFLAGS =
TARGETS = supervisor worker

all: $(TARGETS)

supervisor: supervisor.c
	$(CC) $(CFLAGS) -o $@ supervisor.c $(LDFLAGS)

worker: worker.c
	$(CC) $(CFLAGS) -o $@ worker.c $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o sup_out.log sup_err.log

.PHONY: all clean
