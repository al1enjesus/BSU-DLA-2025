CC = g++
CFLAGS = -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=35 -Wall -Wextra -g -std=c++17
LIBS = -lfuse3 -lpthread
TARGETS = passthrough_fs rot13_fs uppercase_fs
BIN_DIR = bin
SRC_DIR = src
OBJ_DIR = obj

.PHONY: all clean prepare test install_deps

all: prepare $(addprefix $(BIN_DIR)/, $(TARGETS))

prepare:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR)

# Объектные файлы для переиспользования кода
$(OBJ_DIR)/common_ops.o: $(SRC_DIR)/common_ops.cpp $(SRC_DIR)/utils.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Правила для каждого исполняемого файла
$(BIN_DIR)/passthrough_fs: $(SRC_DIR)/passthrough_fs.cpp $(SRC_DIR)/utils.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) -Wno-missing-field-initializers

$(BIN_DIR)/rot13_fs: $(SRC_DIR)/rot13_fs.cpp $(SRC_DIR)/utils.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) -Wno-missing-field-initializers

$(BIN_DIR)/uppercase_fs: $(SRC_DIR)/uppercase_fs.cpp $(SRC_DIR)/utils.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS) -Wno-missing-field-initializers

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

distclean: clean
	rm -rf *.o *~ core

test: all
	@echo "Build successful!"
	@echo "Run: ./$(BIN_DIR)/passthrough_fs /tmp/source /tmp/mount -f"

install_deps:
	sudo apt install -y libfuse3-dev g++ make

# Не включать бинарные файлы в git
.gitignore:
	echo "$(BIN_DIR)/" >> .gitignore
	echo "$(OBJ_DIR)/" >> .gitignore
	echo "*.o" >> .gitignore
	echo "*~" >> .gitignore
	echo "core" >> .gitignore