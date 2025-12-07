CC = g++
CFLAGS = -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=35 -Wall -Wextra -g
LIBS = -lfuse3 -lpthread
TARGETS = bin/passthrough_fs bin/rot13_fs bin/uppercase_fs
BIN_DIR = bin
SRC_DIR = src

.PHONY: all clean prepare test

all: prepare $(TARGETS)

prepare:
	@mkdir -p $(BIN_DIR)

$(BIN_DIR)/passthrough_fs: $(SRC_DIR)/passthrough_fs.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) -Wno-missing-field-initializers -Wno-unused-parameter

$(BIN_DIR)/rot13_fs: $(SRC_DIR)/rot13_fs.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) -Wno-missing-field-initializers -Wno-unused-parameter

$(BIN_DIR)/uppercase_fs: $(SRC_DIR)/uppercase_fs.cpp
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) -Wno-missing-field-initializers -Wno-unused-parameter

clean:
	rm -rf $(BIN_DIR)

test: all
	@echo "Build successful!"
	@echo "To test passthrough FS:"
	@echo "  mkdir -p /tmp/source /tmp/mount"
	@echo "  ./bin/passthrough_fs /tmp/source /tmp/mount -f"
	@echo ""
	@echo "To test ROT13 FS:"
	@echo "  ./bin/rot13_fs /tmp/source /tmp/mount -f"
	@echo ""
	@echo "To test uppercase FS:"
	@echo "  ./bin/uppercase_fs /tmp/source /tmp/mount -f"

install_deps:
	sudo apt install -y libfuse3-dev g++ make