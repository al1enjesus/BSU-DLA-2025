CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64
FUSE_CFLAGS = $(shell pkg-config fuse3 --cflags)
FUSE_LIBS = $(shell pkg-config fuse3 --libs)

TARGETS = passthrough_fs rot13_fs uppercase_fs

.PHONY: all clean

all: $(TARGETS)

passthrough_fs: passthrough_fs.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o $@ $< $(FUSE_LIBS)

rot13_fs: rot13_fs.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o $@ $< $(FUSE_LIBS)

uppercase_fs: uppercase_fs.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o $@ $< $(FUSE_LIBS)

clean:
	rm -f $(TARGETS)
