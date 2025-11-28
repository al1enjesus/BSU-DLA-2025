CC = gcc
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64
LIBS = -lfuse3

all: passthrough archive monitor

passthrough: src/passthrough.c src/common.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

archive: src/archive.c src/common.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

monitor: src/monitor.c src/common.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f passthrough archive monitor

install-deps:
	sudo apt install libfuse3-dev fuse3