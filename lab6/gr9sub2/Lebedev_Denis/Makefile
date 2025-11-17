CC=gcc
CFLAGS=$(shell pkg-config --cflags fuse3) -D_FILE_OFFSET_BITS=64
LIBS=$(shell pkg-config --libs fuse3)

SRC_DIR=src
BUILD_DIR=build
TARGETS = task_c task_a task_b

all: $(TARGETS:%=$(BUILD_DIR)/%)

$(BUILD_DIR)/task_c: $(BUILD_DIR)/task_c.o
	$(CC) -o $@ $^ $(LIBS)

$(BUILD_DIR)/task_a: $(BUILD_DIR)/task_a.o
	$(CC) -o $@ $^ $(LIBS)

$(BUILD_DIR)/task_b: $(BUILD_DIR)/task_b.o
	$(CC) -o $@ $^ $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
