CC = gcc
# Устанавливаем версию API FUSE (libfuse3 требует значение >= 30)
CFLAGS = -Wall -Wextra -g -DFUSE_USE_VERSION=35 $(shell pkg-config --cflags fuse3)
LDFLAGS = $(shell pkg-config --libs fuse3)

SRC = src/myfuse.c src/operations.c src/utils.c
OBJ = $(SRC:.c=.o)

all: myfuse

myfuse: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) myfuse

.PHONY: all clean
