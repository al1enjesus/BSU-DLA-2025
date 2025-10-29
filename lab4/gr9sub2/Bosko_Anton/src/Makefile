CC = gcc
CFLAGS = -O2 -fPIC -Wall -Wextra -g
LDFLAGS = -ldl

SRC_DIR = src
BUILD_DIR = build

all: dirs $(BUILD_DIR)/libsyscall_spy.so $(BUILD_DIR)/benchmark

dirs:
	mkdir -p $(BUILD_DIR)

# LD_PRELOAD library
$(BUILD_DIR)/libsyscall_spy.so: $(SRC_DIR)/syscall_spy.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

# Benchmark binary
$(BUILD_DIR)/benchmark: $(SRC_DIR)/benchmark.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean dirs

