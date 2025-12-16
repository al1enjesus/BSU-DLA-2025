CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -g
FUSE_CFLAGS := $(shell pkg-config fuse3 --cflags)
FUSE_LIBS   := $(shell pkg-config fuse3 --libs)

SRC_DIR := src

BIN_PASSTHROUGH := myfuse
BIN_MONITOR     := monitor_fs
BIN_ARCHIVE     := archive_fs

all: $(BIN_PASSTHROUGH) $(BIN_MONITOR) $(BIN_ARCHIVE)

$(BIN_PASSTHROUGH): $(SRC_DIR)/myfuse.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) $< -o $@ $(FUSE_LIBS)

# Monitoring FS
$(BIN_MONITOR): $(SRC_DIR)/monitor_fs.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) $< -o $@ $(FUSE_LIBS)

$(BIN_ARCHIVE): $(SRC_DIR)/archive_fs.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) $< -o $@ $(FUSE_LIBS)

clean:
	rm -f $(BIN_PASSTHROUGH) $(BIN_MONITOR) $(BIN_ARCHIVE)

.PHONY: all clean