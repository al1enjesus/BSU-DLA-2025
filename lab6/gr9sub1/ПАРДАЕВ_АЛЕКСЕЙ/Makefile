CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64
CFLAGS += -DFUSE_USE_VERSION=35

FUSE_CFLAGS = $(shell pkg-config fuse3 --cflags)
FUSE_LIBS   = $(shell pkg-config fuse3 --libs)

# если нет fuse3 — используем fuse2
ifeq ($(FUSE_LIBS),)
	FUSE_CFLAGS = $(shell pkg-config fuse --cflags)
	FUSE_LIBS   = $(shell pkg-config fuse --libs)
endif

SRC = src/main.c src/utils.c \
      src/passthrough.c \
      src/archive_fs.c \
      src/monitoring_fs.c

TARGET = myfuse

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o $@ $(SRC) $(FUSE_LIBS)

clean:
	rm -f $(TARGET)
