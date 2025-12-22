CC = gcc
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 $(shell pkg-config --cflags fuse3)
LDFLAGS = $(shell pkg-config --libs fuse3)

# For archive
ARCHIVE_CFLAGS = $(shell pkg-config --cflags libarchive)
ARCHIVE_LDFLAGS = $(shell pkg-config --libs libarchive)

all: passthrough archive monitoring

passthrough: passthrough.c
	$(CC) $(CFLAGS) -o ../bin/passthrough passthrough.c $(LDFLAGS)

archive: archive.c
	$(CC) $(CFLAGS) $(ARCHIVE_CFLAGS) -o ../bin/archive archive.c $(LDFLAGS) $(ARCHIVE_LDFLAGS)

monitoring: monitoring.c
	$(CC) $(CFLAGS) -o ../bin/monitoring monitoring.c $(LDFLAGS)

clean:
	rm -f ../bin/passthrough ../bin/archive ../bin/monitoring

.PHONY: all clean