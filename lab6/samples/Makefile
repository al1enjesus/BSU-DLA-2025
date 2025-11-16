# Makefile для примера FUSE filesystem

CC = gcc
CFLAGS = -Wall -Wextra -D_FILE_OFFSET_BITS=64
FUSE_CFLAGS = $(shell pkg-config fuse3 --cflags)
FUSE_LIBS = $(shell pkg-config fuse3 --libs)

# Если fuse3 не найден, попробовать fuse (версия 2)
ifeq ($(FUSE_LIBS),)
    FUSE_CFLAGS = $(shell pkg-config fuse --cflags)
    FUSE_LIBS = $(shell pkg-config fuse --libs)
endif

TARGET = passthrough_fuse
SOURCES = passthrough_fuse.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	@echo "Compiling $(TARGET)..."
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o $(TARGET) $(SOURCES) $(FUSE_LIBS)
	@echo "Build complete: ./$(TARGET)"
	@echo ""
	@echo "Usage: ./$(TARGET) <source_dir> <mount_point> [options]"
	@echo "Example: ./$(TARGET) /tmp/source /mnt/fuse -f"
	@echo ""
	@echo "To unmount: fusermount -u /mnt/fuse"

clean:
	rm -f $(TARGET)
	@echo "Cleaned."

test: $(TARGET)
	@echo "=== Testing FUSE filesystem ==="
	@mkdir -p /tmp/fuse_test_src /tmp/fuse_test_mnt
	@echo "Starting FUSE in background..."
	@./$(TARGET) /tmp/fuse_test_src /tmp/fuse_test_mnt -f &
	@sleep 2
	@echo "Testing operations..."
	@echo "Hello FUSE" > /tmp/fuse_test_mnt/test.txt
	@cat /tmp/fuse_test_mnt/test.txt
	@ls -la /tmp/fuse_test_mnt/
	@rm /tmp/fuse_test_mnt/test.txt
	@echo "Unmounting..."
	@fusermount -u /tmp/fuse_test_mnt || umount /tmp/fuse_test_mnt
	@rmdir /tmp/fuse_test_mnt /tmp/fuse_test_src
	@echo "Test complete!"

help:
	@echo "Available targets:"
	@echo "  make          - Build the FUSE filesystem"
	@echo "  make clean    - Remove built files"
	@echo "  make test     - Run basic tests"
	@echo "  make help     - Show this help"

.PHONY: all clean test help
