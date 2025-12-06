CC = g++
CFLAGS = -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=35 -Wall -Wextra
LIBS = -lfuse3
TARGETS = passthrough rot13_fs uppercase_fs

all: $(TARGETS)

passthrough: passthrough.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

rot13_fs: rot13_fs.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

uppercase_fs: uppercase_fs.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGETS) *.o

test: all
	@echo "Build successful! To test:"
	@echo "  1. mkdir -p /tmp/source /tmp/fuse"
	@echo "  2. ./passthrough /tmp/source /tmp/fuse -f"
	@echo "  3. In another terminal: echo 'test' > /tmp/fuse/test.txt"

.PHONY: all clean test
