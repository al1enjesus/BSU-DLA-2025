CC = gcc
CFLAGS = -Wall -Wextra -g -D_FILE_OFFSET_BITS=64 `pkg-config --cflags fuse3`
LIBS = `pkg-config --libs fuse3`
TARGET = myfuse
SOURCES = src/myfuse.c src/operations.c src/utils.c
OBJS = $(SOURCES:.c=.o)

# Константы для улучшения читаемости
INSTALL_CMD = sudo apt-get install
FUSE_PACKAGES = libfuse3-dev fuse3

.PHONY: all clean check-deps install-deps test

all: check-deps $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

check-deps:
	@pkg-config --exists fuse3 || (echo "fuse3-dev не установлен. Запустите: make install-deps" && exit 1)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

test: $(TARGET)
	@echo "Запуск базовых тестов..."
	@mkdir -p /tmp/test_source /tmp/test_mount
	@echo "test content" > /tmp/test_source/test.txt
	@echo "Тестирование монтирования..."
	@./$(TARGET) /tmp/test_source /tmp/test_mount &
	@sleep 2
	@echo "Тестирование операций..."
	@cat /tmp/test_mount/test.txt > /dev/null && echo "✓ Чтение работает" || echo "✗ Чтение не работает"
	@ls /tmp/test_mount/ > /dev/null && echo "✓ Листинг работает" || echo "✗ Листинг не работает"
	@fusermount3 -u /tmp/test_mount && echo "✓ Демонтирование работает" || echo "✗ Демонтирование не работает"
	@rm -rf /tmp/test_source /tmp/test_mount

install-deps:
	$(INSTALL_CMD) update
	$(INSTALL_CMD) install $(FUSE_PACKAGES) pkg-config

debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)

release: CFLAGS += -O2 -DNDEBUG
release: $(TARGET)