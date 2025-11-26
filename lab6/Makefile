CC = gcc
CFLAGS = -Wall -Wextra -g `pkg-config --cflags fuse3`
LDFLAGS = `pkg-config --libs fuse3`
SOURCES = src/myfuse.c src/operations.c src/utils.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = myfuse

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean