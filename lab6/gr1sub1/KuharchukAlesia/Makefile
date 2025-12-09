CC = gcc
CFLAGS = -Wall -Wextra -g -D_FILE_OFFSET_BITS=64
LIBS = -lfuse3 -lpthread

all: myfuse archive_fs_simple monitoring_fs

myfuse: simple_fuse.c
	$(CC) $(CFLAGS) -o $@ $< -lfuse3

archive_fs_simple: archive_fs_fixed.c
	$(CC) $(CFLAGS) -o $@ $< -lfuse3

monitoring_fs: monitoring_fs.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f myfuse archive_fs_simple monitoring_fs

.PHONY: all clean
