CC = g++
CFLAGS = -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=35
LIBS = -lfuse3
TARGETS = passthrough archive_fs monitor_fs

all: $(TARGETS)

passthrough: passthrough.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

archive_fs: archive_fs.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

monitor_fs: monitor_fs.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
