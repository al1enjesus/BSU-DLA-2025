# Makefile
# Используем fuse (обычно соответствует libfuse2, которая стоит в системе)
FUSE_VER = fuse 

# Определяем используемые компиляторы и флаги
# ДОБАВЛЯЕМ -D_FILE_OFFSET_BITS=64, как требует FUSE
CC = gcc
CFLAGS = -Wall -g -std=c99 -D_FILE_OFFSET_BITS=64 
# Флаги для libfuse
FUSE_CFLAGS = $(shell pkg-config --cflags $(FUSE_VER))
FUSE_LIBS = $(shell pkg-config --libs $(FUSE_VER))

# Имя исполняемого файла
TARGET = myfuse

# Исходные файлы
SRCS = src/myfuse.c src/operations.c src/utils.c
OBJS = $(patsubst %.c, %.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

# Сборка исполняемого файла
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(FUSE_LIBS)

# Компиляция объектных файлов
src/%.o: src/%.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
	rm -f src/*.o