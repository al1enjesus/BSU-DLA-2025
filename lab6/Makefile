CC = gcc
CFLAGS = -Wall -Wextra -g -D_FILE_OFFSET_BITS=64 `pkg-config --cflags fuse3`
LIBS = `pkg-config --libs fuse3`
TARGET = myfuse
SOURCES = src/myfuse.c src/operations.c src/utils.c
OBJS = $(SOURCES:.c=.o)

.PHONY: all clean check-deps

all: check-deps $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

check-deps:
	@pkg-config --exists fuse3 || (echo "fuse3-dev не установлен. Установите: sudo apt-get install libfuse3-dev fuse3" && exit 1)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

test: $(TARGET)
	@echo "Запуск тестов..."
	# Добавьте тестовые команды здесь

install-deps:
	sudo apt-get update
	sudo apt-get install libfuse3-dev fuse3 pkg-config