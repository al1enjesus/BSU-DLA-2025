CC = gcc
CFLAGS = -Wall -Wextra -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -g -I./src
FUSE_CFLAGS = $(shell pkg-config fuse3 --cflags 2>/dev/null || echo "-I/usr/include/fuse3")
FUSE_LIBS = $(shell pkg-config fuse3 --libs 2>/dev/null || echo "-lfuse3 -lpthread")

# Проверка наличия FUSE
ifeq ($(shell pkg-config fuse3 --exists 2>/dev/null; echo $$?),1)
    $(info FUSE3 not found via pkg-config, trying libfuse...)
    FUSE_CFLAGS = $(shell pkg-config fuse --cflags 2>/dev/null || echo "-I/usr/include/fuse")
    FUSE_LIBS = $(shell pkg-config fuse --libs 2>/dev/null || echo "-lfuse -lpthread")
endif

TARGET = myfuse
SOURCES = src/main.c src/operations.c src/utils.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CC) $(CFLAGS) -o $@ $^ $(FUSE_LIBS)
	@echo "Build successful!"
	@echo ""
	@echo "Usage: ./$(TARGET) <source_dir> <mount_point> [options]"
	@echo "Example: ./$(TARGET) /tmp/source /mnt/fuse -f"

%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning up..."
	rm -f $(TARGET) $(OBJECTS)
	@echo "Done."
