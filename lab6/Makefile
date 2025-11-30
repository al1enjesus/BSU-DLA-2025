CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_FILE_OFFSET_BITS=64
# Изменение: используем FUSE 2
CFLAGS += -DFUSE_USE_VERSION=26 

# Изменение: используем pkg-config fuse (FUSE 2)
# -larchive был добавлен в FUSE_LIBS в предыдущей версии, оставим его,
# но убедимся, что CFLAGS берутся корректно (они не нужны для libarchive)
FUSE_CFLAGS = $(shell pkg-config fuse --cflags)
FUSE_LIBS   = $(shell pkg-config fuse --libs) -larchive

# Новые имена исходных файлов
SRC = src/main.c src/utils.c \
      src/task_proxy.c \
      src/task_archive.c \
      src/task_monitor.c

TARGET = myfuse

all: $(TARGET)

$(TARGET): $(SRC)
	# Добавляем CFLAGS FUSE и LIBS FUSE/libarchive
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -o $@ $(SRC) $(FUSE_LIBS)

clean:
	rm -f $(TARGET)