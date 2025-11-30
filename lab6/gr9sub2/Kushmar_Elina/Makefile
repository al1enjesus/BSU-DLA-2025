CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64
FUSE_CFLAGS = $(shell pkg-config fuse3 --cflags 2>/dev/null || pkg-config fuse --cflags)
FUSE_LIBS   = $(shell pkg-config fuse3 --libs 2>/dev/null || pkg-config fuse --libs)

TARGETS = passthrough archive_fs monitoring_fs

all: $(TARGETS)

passthrough: passthrough_fuse.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o passthrough passthrough_fuse.c $(FUSE_LIBS)

archive_fs: archive_fs.c tar.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o archive_fs archive_fs.c tar.c $(FUSE_LIBS)

monitoring_fs: monitoring_fs.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o monitoring_fs monitoring_fs.c $(FUSE_LIBS)

test: passthrough
	mkdir -p /tmp/source /mnt/fuse
	./passthrough /tmp/source /mnt/fuse -f &

clean:
	rm -f $(TARGETS)
	-fusermount -u /mnt/fuse 2>/dev/null || true
	-fusermount -u /mnt/archive 2>/dev/null || true
	-fusermount -u /mnt/mon 2>/dev/null || true

.PHONY: all clean test