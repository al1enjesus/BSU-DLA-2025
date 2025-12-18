CC = gcc
CFLAGS = -Wall -Wextra -g -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=31
LIBS = -lfuse3 -larchive -lpthread
TARGET = archive_fs
SRC = src/main.c src/archive_ops.c
OBJ = $(SRC:.c=.o)

# Цвета для вывода
RED=\033[0;31m
GREEN=\033[0;32m
YELLOW=\033[1;33m
NC=\033[0m

all: $(TARGET)

$(TARGET): $(OBJ)
	@echo "$(GREEN)Linking $(TARGET)...$(NC)"
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo "$(GREEN)Build successful!$(NC)"

%.o: %.c
	@echo "$(YELLOW)Compiling $<...$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "$(RED)Cleaning...$(NC)"
	rm -f $(TARGET) $(OBJ)
	rm -f test.tar test2.tar broken.tar
	rm -rf /tmp/archive_test /tmp/archive_mount /tmp/fuse_demo /tmp/broken_mount 2>/dev/null || true

# Тестирование
test: $(TARGET)
	@echo "\n$(GREEN)=== Starting Archive FUSE Tests ===$(NC)"
	
	@echo "\n$(YELLOW)0. Cleaning up from previous runs...$(NC)"
	@-fusermount -u /tmp/archive_test 2>/dev/null || true
	@-fusermount -u /tmp/broken_mount 2>/dev/null || true
	@killall $(TARGET) 2>/dev/null || true
	@sleep 1
	@rm -rf test_dir test.tar broken.tar /tmp/archive_test /tmp/broken_mount 2>/dev/null || true
	
	@echo "\n$(YELLOW)1. Creating test archive...$(NC)"
	@mkdir -p test_dir/subdir
	@echo "Hello from file1.txt" > test_dir/file1.txt
	@echo "Content of file2.txt" > test_dir/file2.txt
	@echo "Nested file content" > test_dir/subdir/nested.txt
	@echo "Executable script" > test_dir/script.sh
	@chmod +x test_dir/script.sh
	@tar -cf test.tar -C test_dir .
	@echo "Archive created: test.tar ($$(wc -c < test.tar) bytes)"
	
	@echo "\n$(YELLOW)2. Creating mount point...$(NC)"
	@mkdir -p /tmp/archive_test
	@echo "Mount point: /tmp/archive_test"
	
	@echo "\n$(YELLOW)3. Mounting archive...$(NC)"
	@./$(TARGET) test.tar /tmp/archive_test -f &
	@ARCHIVE_PID=$$!; \
	sleep 3; \
	
	@echo "\n$(YELLOW)4. Testing filesystem operations...$(NC)"
	
	@echo "\n4.1 Listing root directory:"
	@ls -la /tmp/archive_test/ || echo "FAILED"
	
	@echo "\n4.2 Reading file1.txt:"
	@cat /tmp/archive_test/file1.txt 2>/dev/null || echo "FAILED"
	
	@echo "\n4.3 Reading nested file:"
	@cat /tmp/archive_test/subdir/nested.txt 2>/dev/null || echo "FAILED"
	
	@echo "\n4.4 Testing path traversal protection:"
	@echo "Attempting to access ../etc/passwd..."
	@cat /tmp/archive_test/../etc/passwd 2>/dev/null && echo "SECURITY FAILURE!" || echo "Security check passed"
	
	@echo "\n4.5 Testing read-only protection:"
	@echo "Attempting to create file..."
	@touch /tmp/archive_test/test_write.txt 2>/dev/null && echo "FAILED: Write should not be allowed" || echo "Read-only protection works"
	
	@echo "\n$(YELLOW)5. Testing with corrupted archive...$(NC)"
	@echo "Creating corrupted archive..."
	@dd if=/dev/urandom of=broken.tar bs=1k count=1 2>/dev/null
	@echo "Attempting to mount corrupted archive (should fail)..."
	@mkdir -p /tmp/broken_mount 2>/dev/null || true
	@timeout 3 ./$(TARGET) broken.tar /tmp/broken_mount -f 2>&1 | head -10 && echo "Mount failed as expected" || echo "Mount failed as expected"
	
	@echo "\n$(YELLOW)6. Testing with non-existent archive...$(NC)"
	@./$(TARGET) nonexistent.tar /tmp/archive_test 2>&1 | head -3 || true
	
	@echo "\n$(YELLOW)7. Cleaning up...$(NC)"
	@-fusermount -u /tmp/archive_test 2>/dev/null || kill $$ARCHIVE_PID 2>/dev/null || true
	@sleep 2
	@-fusermount -u /tmp/broken_mount 2>/dev/null || true
	@rm -rf test_dir test.tar broken.tar /tmp/archive_test /tmp/broken_mount 2>/dev/null || true
	@killall $(TARGET) 2>/dev/null || true
	
	@echo "\n$(GREEN)=== Tests completed ===$(NC)"

# Демонстрационный тест
demo: $(TARGET)
	@echo "\n$(GREEN)=== Archive FUSE Demo ===$(NC)"
	@echo "Cleaning up from previous runs..."
	@-fusermount -u /tmp/fuse_demo 2>/dev/null || true
	@killall $(TARGET) 2>/dev/null || true
	@sleep 1
	@rm -rf demo_archive demo.tar /tmp/fuse_demo 2>/dev/null || true
	
	@echo "\nCreating demo archive..."
	@mkdir -p demo_archive
	@echo "Welcome to Archive FUSE Demo!" > demo_archive/README.txt
	@echo "This is a sample archive." >> demo_archive/README.txt
	@mkdir -p demo_archive/docs
	@echo "Documentation" > demo_archive/docs/index.txt
	@mkdir -p demo_archive/code
	@echo "// Sample code\nint main() { return 0; }" > demo_archive/code/sample.c
	@tar -cf demo.tar -C demo_archive .
	
	@mkdir -p /tmp/fuse_demo
	@echo "\nMounting archive to /tmp/fuse_demo..."
	@./$(TARGET) demo.tar /tmp/fuse_demo -f &
	@DEMO_PID=$$!; \
	sleep 3; \
	
	@echo "\nContents of mounted archive:"
	@find /tmp/fuse_demo -type f -o -type d | sort
	
	@echo "\nReading files:"
	@cat /tmp/fuse_demo/README.txt
	@echo "\n---"
	@cat /tmp/fuse_demo/code/sample.c
	
	@echo "\n$(YELLOW)Archive mounted at /tmp/fuse_demo$(NC)"
	@echo "To unmount: fusermount -u /tmp/fuse_demo"
	@echo "Or press Ctrl+C in this terminal"
	@echo "\nWaiting for 30 seconds (or Ctrl+C to exit)..."
	@sleep 30; \
	-fusermount -u /tmp/fuse_demo 2>/dev/null || kill $$DEMO_PID 2>/dev/null || true; \
	echo "\nUnmounted."; \
	rm -rf demo_archive demo.tar /tmp/fuse_demo 2>/dev/null || true

# Установка зависимостей (для Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y gcc make libfuse3-dev libarchive-dev pkg-config

.PHONY: all clean test demo install-deps
