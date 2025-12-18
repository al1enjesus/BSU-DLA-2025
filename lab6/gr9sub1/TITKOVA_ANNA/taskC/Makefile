# Makefile для Monitoring FUSE Filesystem

CC = gcc
CFLAGS = -Wall -Wextra -D_FILE_OFFSET_BITS=64 -std=c11 -g -pedantic -D_GNU_SOURCE
FUSE_CFLAGS = $(shell pkg-config fuse3 --cflags 2>/dev/null || pkg-config fuse --cflags 2>/dev/null || echo "-I/usr/include/fuse3")
FUSE_LIBS = $(shell pkg-config fuse3 --libs 2>/dev/null || pkg-config fuse --libs 2>/dev/null || echo "-lfuse3 -lpthread")

# Проверка наличия FUSE
ifeq ($(FUSE_LIBS),)
    $(error FUSE library not found. Please install libfuse3-dev: sudo apt-get install libfuse3-dev)
endif

TARGET = monitoring_fuse
SOURCES = src/monitoring_fuse.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean install 

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	@$(CC) $(CFLAGS) -o $@ $^ $(FUSE_LIBS)
	@echo "Build successful!"
	@echo ""
	@echo "Usage: ./$(TARGET) <source_dir> <mount_point> [options]"
	@echo "Example: ./$(TARGET) /tmp/source /mnt/monitor -f -d"
	@echo "Virtual stats file will be available at /mnt/monitor/.stats"

%.o: %.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) $(FUSE_CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning up..."
	@rm -f $(TARGET) $(OBJECTS) *.log test_*.txt *.core
	@rm -rf /tmp/fuse_test_* /tmp/bench_* 2>/dev/null || true
	@echo "Done."




