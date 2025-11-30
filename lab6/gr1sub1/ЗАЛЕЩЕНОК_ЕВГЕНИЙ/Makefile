CC = gcc
CFLAGS = -Wall -g $(shell pkg-config fuse3 --cflags)
LDFLAGS = $(shell pkg-config fuse3 --libs)

TARGETS = myfuse archivefs

all: $(TARGETS)

myfuse: src/myfuse.c
	$(CC) $(CFLAGS) -o myfuse src/myfuse.c $(LDFLAGS)

archivefs: src/archivefs.c
	$(CC) $(CFLAGS) -o archivefs src/archivefs.c $(LDFLAGS)

clean:
	rm -f $(TARGETS)
