CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64
FUSE_CFLAGS = $(shell pkg-config fuse3 --cflags)
FUSE_LIBS   = $(shell pkg-config fuse3 --libs)

TARGETS = passthrough archive_fs monitoring_fs

all: $(TARGETS)

passthrough: src/passthrough.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o passthrough src/passthrough.c $(FUSE_LIBS)

archive_fs: src/archive_fs.c src/tar.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o archive_fs src/archive_fs.c src/tar.c $(FUSE_LIBS)

monitoring_fs: src/monitoring_fs.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o monitoring_fs src/monitoring_fs.c $(FUSE_LIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
