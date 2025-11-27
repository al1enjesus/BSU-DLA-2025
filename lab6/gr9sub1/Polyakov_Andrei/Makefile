# Makefile для FUSE Filesystem Lab

CC = gcc
CFLAGS = -Wall -Wextra -O2 `pkg-config fuse3 --cflags`
LDFLAGS = `pkg-config fuse3 --libs` -larchive -lpthread

BUILDDIR = build
SRCDIR = src

# Исходные файлы
PASSTHROUGH_SRC = $(SRCDIR)/passthrough_fs.c
ARCHIVE_SRC = $(SRCDIR)/archive_fs.c
MONITORING_SRC = $(SRCDIR)/monitoring_fs.c

# Исполняемые файлы
PASSTHROUGH_BIN = $(BUILDDIR)/passthrough_fs
ARCHIVE_BIN = $(BUILDDIR)/archive_fs
MONITORING_BIN = $(BUILDDIR)/monitoring_fs

# Все цели
ALL_BINS = $(PASSTHROUGH_BIN) $(ARCHIVE_BIN) $(MONITORING_BIN)

.PHONY: all clean test setup

all: $(BUILDDIR) $(ALL_BINS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Task A: Passthrough Filesystem
$(PASSTHROUGH_BIN): $(PASSTHROUGH_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# Task B: Archive Filesystem
$(ARCHIVE_BIN): $(ARCHIVE_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# Task C: Monitoring Filesystem
$(MONITORING_BIN): $(MONITORING_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

clean:
	rm -rf $(BUILDDIR)
	@echo "Cleaned build directory"

# Настройка структуры проекта
setup:
	mkdir -p $(SRCDIR)
	mkdir -p $(BUILDDIR)
	@echo "Project structure created"

# Тестирование Task A
test-passthrough: $(PASSTHROUGH_BIN)
	@echo "=== Testing Passthrough FS ==="
	mkdir -p /tmp/pass_source /tmp/pass_mnt
	./$(PASSTHROUGH_BIN) /tmp/pass_source /tmp/pass_mnt &
	@sleep 1
	@echo "Creating test file..."
	@echo "Hello FUSE!" > /tmp/pass_mnt/test.txt
	@echo "Reading test file..."
	@cat /tmp/pass_mnt/test.txt
	@echo "Listing files..."
	@ls -la /tmp/pass_mnt/
	@echo "Removing test file..."
	@rm -f /tmp/pass_mnt/test.txt
	@fusermount3 -u /tmp/pass_mnt || true
	@echo "Test completed!"

# Тестирование Task B
test-archive: $(ARCHIVE_BIN)
	@echo "=== Testing Archive FS ==="
	mkdir -p /tmp/test_archive
	echo "Test file 1" > /tmp/test_archive/file1.txt
	echo "Test file 2" > /tmp/test_archive/file2.txt
	mkdir -p /tmp/test_archive/subdir
	echo "File in subdir" > /tmp/test_archive/subdir/file3.txt
	tar -cf /tmp/test.tar -C /tmp/test_archive .
	mkdir -p /tmp/archive_mnt
	./$(ARCHIVE_BIN) /tmp/test.tar /tmp/archive_mnt &
	@sleep 1
	@echo "Listing archive contents..."
	@ls -la /tmp/archive_mnt/
	@echo "Reading file from archive..."
	@cat /tmp/archive_mnt/file1.txt
	@fusermount3 -u /tmp/archive_mnt || true
	@rm -rf /tmp/test_archive /tmp/test.tar
	@echo "Test completed!"

# Тестирование Task C
test-monitoring: $(MONITORING_BIN)
	@echo "=== Testing Monitoring FS ==="
	mkdir -p /tmp/mon_source /tmp/mon_mnt
	./$(MONITORING_BIN) /tmp/mon_source /tmp/mon_mnt &
	@sleep 1
	@echo "Creating files..."
	@echo "Test data" > /tmp/mon_mnt/file1.txt
	@echo "More data" > /tmp/mon_mnt/file2.txt
	@echo "Reading files..."
	@cat /tmp/mon_mnt/file1.txt > /dev/null
	@cat /tmp/mon_mnt/file2.txt > /dev/null
	@echo "Checking statistics..."
	@cat /tmp/mon_mnt/.stats
	@fusermount3 -u /tmp/mon_mnt || true
	@echo "Test completed!"

# Запуск всех тестов
test: test-passthrough test-archive test-monitoring

# Информация о сборке
info:
	@echo "FUSE Filesystem Lab - Build Information"
	@echo "========================================"
	@echo "Compiler: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build all programs"
	@echo "  clean            - Remove build files"
	@echo "  test             - Run all tests"
	@echo "  test-passthrough - Test Task A"
	@echo "  test-archive     - Test Task B"
	@echo "  test-monitoring  - Test Task C"
	@echo "  setup            - Create directory structure"
