# Makefile для лабораторной работы 6: FUSE файловые системы

# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_FILE_OFFSET_BITS=64
LIBS = -lfuse3
PKG_CONFIG = `pkg-config fuse3 --cflags --libs`

# Директории
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Исходные файлы
COMMON_SOURCES = $(SRCDIR)/operations.c $(SRCDIR)/utils.c
PASSTHROUGH_SOURCES = $(SRCDIR)/passthrough_fs.c $(COMMON_SOURCES)
ROT13_SOURCES = $(SRCDIR)/rot13_fs.c $(COMMON_SOURCES)
UPPERCASE_SOURCES = $(SRCDIR)/uppercase_fs.c $(COMMON_SOURCES)

# Объектные файлы
COMMON_OBJECTS = $(OBJDIR)/operations.o $(OBJDIR)/utils.o
PASSTHROUGH_OBJECTS = $(OBJDIR)/passthrough_fs.o $(COMMON_OBJECTS)
ROT13_OBJECTS = $(OBJDIR)/rot13_fs.o $(COMMON_OBJECTS)
UPPERCASE_OBJECTS = $(OBJDIR)/uppercase_fs.o $(COMMON_OBJECTS)

# Исполняемые файлы
PASSTHROUGH_TARGET = $(BINDIR)/passthrough_fs
ROT13_TARGET = $(BINDIR)/rot13_fs
UPPERCASE_TARGET = $(BINDIR)/uppercase_fs

# Цели по умолчанию
.PHONY: all clean help test setup directories

all: directories $(PASSTHROUGH_TARGET) $(ROT13_TARGET) $(UPPERCASE_TARGET)

# Создание директорий
directories:
	@mkdir -p $(OBJDIR) $(BINDIR)

# Задание A: Passthrough filesystem
$(PASSTHROUGH_TARGET): $(PASSTHROUGH_OBJECTS)
	@echo "Сборка passthrough filesystem..."
	$(CC) $(PASSTHROUGH_OBJECTS) -o $@ $(PKG_CONFIG)

# Задание B: ROT13 encryption filesystem  
$(ROT13_TARGET): $(ROT13_OBJECTS)
	@echo "Сборка ROT13 encryption filesystem..."
	$(CC) $(ROT13_OBJECTS) -o $@ $(PKG_CONFIG)

# Задание C: Uppercase filesystem
$(UPPERCASE_TARGET): $(UPPERCASE_OBJECTS)
	@echo "Сборка uppercase filesystem..."
	$(CC) $(UPPERCASE_OBJECTS) -o $@ $(PKG_CONFIG)

# Компиляция объектных файлов
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@echo "Компиляция $<..."
	$(CC) $(CFLAGS) `pkg-config fuse3 --cflags` -c $< -o $@

# Установка зависимостей (для Ubuntu/Debian)
install-deps:
	@echo "Установка зависимостей FUSE..."
	sudo apt-get update
	sudo apt-get install libfuse3-dev fuse3 pkg-config

# Проверка зависимостей
check-deps:
	@echo "Проверка зависимостей..."
	@pkg-config --exists fuse3 && echo "✓ libfuse3 установлена" || echo "✗ libfuse3 не найдена"
	@which fusermount && echo "✓ fusermount найден" || echo "✗ fusermount не найден"
	@ls /dev/fuse >/dev/null 2>&1 && echo "✓ /dev/fuse доступен" || echo "✗ /dev/fuse недоступен"

# Настройка тестовых директорий
setup:
	@echo "Создание тестовых директорий..."
	mkdir -p /tmp/lab6_source /tmp/lab6_mount
	echo "Тестовые директории созданы:"
	echo "  Источник: /tmp/lab6_source"
	echo "  Точка монтирования: /tmp/lab6_mount"
	@echo ""
	@echo "Пример использования:"
	@echo "  ./$(PASSTHROUGH_TARGET) /tmp/lab6_source /tmp/lab6_mount"

# Базовое тестирование
test: all setup
	@echo "Запуск базового тестирования..."
	@echo ""
	@echo "1. Создание тестовых файлов..."
	echo "Hello World" > /tmp/lab6_source/test.txt
	echo "Secret message" > /tmp/lab6_source/secret.txt
	mkdir -p /tmp/lab6_source/subdir
	echo "Subdirectory file" > /tmp/lab6_source/subdir/file.txt
	@echo ""
	@echo "Тестовые файлы созданы в /tmp/lab6_source"
	@echo "Для ручного тестирования запустите:"
	@echo "  ./$(PASSTHROUGH_TARGET) /tmp/lab6_source /tmp/lab6_mount"
	@echo "  ./$(ROT13_TARGET) /tmp/lab6_source /tmp/lab6_mount"  
	@echo "  ./$(UPPERCASE_TARGET) /tmp/lab6_source /tmp/lab6_mount"

# Демонстрация ROT13
demo-rot13: $(ROT13_TARGET)
	@echo "Демонстрация ROT13 filesystem..."
	@echo ""
	@echo "Оригинальные файлы:"
	@cat /tmp/lab6_source/test.txt 2>/dev/null || echo "Создайте тестовые файлы: make test"
	@echo ""
	@echo "Для просмотра зашифрованных данных запустите в другом терминале:"
	@echo "  ./$(ROT13_TARGET) /tmp/lab6_source /tmp/lab6_mount -f"
	@echo ""
	@echo "Затем попробуйте:"
	@echo "  cat /tmp/lab6_mount/test.txt     # Расшифрованный текст"
	@echo "  echo 'New data' > /tmp/lab6_mount/new.txt"
	@echo "  cat /tmp/lab6_source/new.txt     # Зашифрованные данные на диске"

# Демонстрация Uppercase
demo-uppercase: $(UPPERCASE_TARGET)
	@echo "Демонстрация Uppercase filesystem..."
	@echo ""
	@echo "Оригинальные файлы:"
	@cat /tmp/lab6_source/test.txt 2>/dev/null || echo "Создайте тестовые файлы: make test"
	@echo ""
	@echo "Для просмотра в верхнем регистре запустите в другом терминале:"
	@echo "  ./$(UPPERCASE_TARGET) /tmp/lab6_source /tmp/lab6_mount -f"
	@echo ""
	@echo "Затем попробуйте:"
	@echo "  cat /tmp/lab6_mount/test.txt     # Текст в верхнем регистре"

# Размонтирование (если зависли)
unmount:
	@echo "Размонтирование FUSE filesystem..."
	-fusermount -u /tmp/lab6_mount 2>/dev/null || true
	-umount /tmp/lab6_mount 2>/dev/null || true
	@echo "Размонтирование завершено"

# Очистка
clean:
	@echo "Очистка сборочных файлов..."
	rm -rf $(OBJDIR) $(BINDIR)
	@echo "Очистка завершена"

clean-all: clean
	@echo "Очистка тестовых данных..."
	rm -rf /tmp/lab6_source /tmp/lab6_mount
	@echo "Полная очистка завершена"

# Справка
help:
	@echo "Лабораторная работа 6: FUSE файловые системы"
	@echo ""
	@echo "Доступные цели:"
	@echo "  all              - Сборка всех программ"
	@echo "  passthrough_fs   - Сборка только passthrough filesystem (Задание A)"
	@echo "  rot13_fs         - Сборка только ROT13 filesystem (Задание B)"
	@echo "  uppercase_fs     - Сборка только uppercase filesystem (Задание C)"
	@echo ""
	@echo "Установка и проверка:"
	@echo "  install-deps     - Установка зависимостей (Ubuntu/Debian)"
	@echo "  check-deps       - Проверка зависимостей"
	@echo ""
	@echo "Тестирование:"
	@echo "  setup            - Создание тестовых директорий"
	@echo "  test             - Базовое тестирование"
	@echo "  demo-rot13       - Демонстрация ROT13"
	@echo "  demo-uppercase   - Демонстрация Uppercase"
	@echo ""
	@echo "Утилиты:"
	@echo "  unmount          - Размонтирование FUSE filesystem"
	@echo "  clean            - Очистка сборочных файлов"
	@echo "  clean-all        - Полная очистка (включая тестовые данные)"
	@echo "  help             - Эта справка"
	@echo ""
	@echo "Пример использования:"
	@echo "  make install-deps   # Установить зависимости"
	@echo "  make all            # Собрать все программы"
	@echo "  make test           # Создать тестовые файлы"
	@echo "  # В терминале 1:"
	@echo "  ./bin/passthrough_fs /tmp/lab6_source /tmp/lab6_mount -f"
	@echo "  # В терминале 2:"
	@echo "  ls /tmp/lab6_mount"
	@echo "  cat /tmp/lab6_mount/test.txt"

# Индивидуальные цели для каждой программы
passthrough_fs: directories $(PASSTHROUGH_TARGET)
rot13_fs: directories $(ROT13_TARGET)  
uppercase_fs: directories $(UPPERCASE_TARGET)

# Зависимости объектных файлов от заголовочных файлов
$(OBJDIR)/operations.o: $(SRCDIR)/operations.h
$(OBJDIR)/utils.o: $(SRCDIR)/operations.h
$(OBJDIR)/passthrough_fs.o: $(SRCDIR)/operations.h
$(OBJDIR)/rot13_fs.o: $(SRCDIR)/operations.h
$(OBJDIR)/uppercase_fs.o: $(SRCDIR)/operations.h