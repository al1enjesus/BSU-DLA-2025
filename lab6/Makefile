CC = gcc
CFLAGS = -Wall -Wextra -g -D_FILE_OFFSET_BITS=64
LDFLAGS = $(shell pkg-config --libs fuse3)
CFLAGS += $(shell pkg-config --cflags fuse3)

TARGET = myfuse
SRCS = main.c passthrough.c archive_fs.c monitoring_fs.c utils.c
OBJS = main.o passthrough.o archive_fs.o monitoring_fs.o utils.o

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

main.o: main.c passthrough.h archive_fs.h monitoring_fs.h
	$(CC) $(CFLAGS) -c -o $@ $<

passthrough.o: passthrough.c passthrough.h utils.h
	$(CC) $(CFLAGS) -c -o $@ $<

archive_fs.o: archive_fs.c archive_fs.h
	$(CC) $(CFLAGS) -c -o $@ $<

monitoring_fs.o: monitoring_fs.c monitoring_fs.h utils.h
	$(CC) $(CFLAGS) -c -o $@ $<

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

# Тестирование passthrough
test-passthrough: $(TARGET)
	@mkdir -p /tmp/fuse_src /tmp/fuse_mnt
	@echo "Mounting passthrough FS..."
	./$(TARGET) passthrough /tmp/fuse_src /tmp/fuse_mnt -f &
	@sleep 1
	@echo "Test: create and read file"
	echo "Hello FUSE" > /tmp/fuse_mnt/test.txt
	cat /tmp/fuse_mnt/test.txt
	rm /tmp/fuse_mnt/test.txt
	@fusermount -u /tmp/fuse_mnt || true

# Тестирование archive
test-archive: $(TARGET)
	@mkdir -p /tmp/fuse_mnt
	@echo "Creating test tar..."
	@echo "test content" > /tmp/testfile.txt
	@tar cf /tmp/test.tar -C /tmp testfile.txt
	@rm /tmp/testfile.txt
	@echo "Mounting archive FS..."
	./$(TARGET) archive /tmp/test.tar /tmp/fuse_mnt -f &
	@sleep 1
	ls /tmp/fuse_mnt/
	cat /tmp/fuse_mnt/testfile.txt
	@fusermount -u /tmp/fuse_mnt || true

