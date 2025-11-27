CC = gcc
CFLAGS = -Wall -Wextra -D_FILE_OFFSET_BITS=64 $(shell pkg-config fuse --cflags)
LDFLAGS = $(shell pkg-config fuse --libs)

TARGET = myfuse
SRCDIR = src
SOURCES = $(SRCDIR)/myfuse.c $(SRCDIR)/operations.c $(SRCDIR)/utils.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -rf /tmp/fuse_source /tmp/fuse_mount

help:
	@echo "Доступные цели:"
	@echo "  make        - собрать проект"
	@echo "  make clean  - удалить бинарники и тестовые директории"
	@echo "  make help   - показать эту справку"