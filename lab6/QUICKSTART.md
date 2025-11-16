# Quick Start Guide - Lab 6 FUSE

Быстрое руководство по началу работы с лабораторной работой 6.

## Установка зависимостей

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install libfuse3-dev fuse3 pkg-config build-essential
```

### Fedora/RHEL
```bash
sudo dnf install fuse3-devel fuse3 pkgconfig gcc make
```

### macOS
```bash
# Установить macFUSE
brew install --cask macfuse

# После установки нужно разрешить kernel extension в System Preferences
# System Preferences → Security & Privacy → Allow macFUSE

# Установить pkg-config
brew install pkg-config
```

### Проверка установки
```bash
# Проверить версию FUSE
pkg-config --modversion fuse3

# Если fuse3 не найден, попробуйте fuse (версия 2)
pkg-config --modversion fuse

# Проверить наличие устройства
ls -l /dev/fuse
# Вывод: crw-rw-rw- 1 root root 10, 229 Jan 15 10:00 /dev/fuse
```

## Компиляция примера

```bash
cd lab6/samples/

# Собрать
make

# Если ошибка про fuse3, попробуйте явно указать fuse2
# gcc -Wall -D_FILE_OFFSET_BITS=64 $(pkg-config fuse --cflags) \
#     -o passthrough_fuse passthrough_fuse.c $(pkg-config fuse --libs)
```

## Запуск примера

### Подготовка
```bash
# Создать директории
mkdir -p /tmp/fuse_source
mkdir -p /tmp/fuse_mount

# Создать тестовые файлы в source
echo "Hello from FUSE" > /tmp/fuse_source/test.txt
mkdir /tmp/fuse_source/subdir
echo "File in subdir" > /tmp/fuse_source/subdir/file.txt
```

### Запуск FUSE в foreground режиме (рекомендуется для отладки)
```bash
./passthrough_fuse /tmp/fuse_source /tmp/fuse_mount -f

# Опции:
# -f  foreground (не уходить в фон, видны логи)
# -d  debug (подробный вывод от libfuse)
```

**В другом терминале** протестируйте:
```bash
# Список файлов
ls -la /tmp/fuse_mount/

# Чтение
cat /tmp/fuse_mount/test.txt

# Запись
echo "New content" > /tmp/fuse_mount/newfile.txt

# Проверка в оригинальной директории
ls /tmp/fuse_source/
cat /tmp/fuse_source/newfile.txt
```

### Размонтирование
```bash
# В терминале с FUSE нажмите Ctrl+C
# Или из другого терминала:
fusermount -u /tmp/fuse_mount

# На macOS:
umount /tmp/fuse_mount
```

### Запуск в background режиме
```bash
# Запустить
./passthrough_fuse /tmp/fuse_source /tmp/fuse_mount

# Проверить
mount | grep fuse

# Размонтировать
fusermount -u /tmp/fuse_mount
```

## Отладка проблем

### Проблема: "fusermount: command not found"
**Linux:**
```bash
sudo apt-get install fuse3  # или fuse
```

**macOS:**
```bash
# fusermount нет на macOS, используйте umount
umount /tmp/fuse_mount
```

### Проблема: "fuse: device not found"
**Linux:**
```bash
# Загрузить модуль ядра
sudo modprobe fuse

# Проверить
lsmod | grep fuse
```

**macOS:**
- Убедитесь, что macFUSE установлен через `brew install --cask macfuse`
- Разрешите kernel extension в System Preferences

### Проблема: "Permission denied" при монтировании
```bash
# Проверить права на /dev/fuse
ls -l /dev/fuse

# Добавить себя в группу fuse (Linux)
sudo usermod -a -G fuse $USER
# Затем перелогиниться
```

### Проблема: "Transport endpoint is not connected"
```bash
# Файловая система уже смонтирована или зависла
# Принудительно размонтировать
fusermount -uz /tmp/fuse_mount

# Или (Linux)
sudo umount -l /tmp/fuse_mount
```

### Проблема: Компиляция не находит fuse.h
```bash
# Проверить установку dev пакета
pkg-config --cflags fuse3

# Если пусто, установить
sudo apt-get install libfuse3-dev

# Для fuse2:
sudo apt-get install libfuse-dev
```

## Структура проекта для сдачи

Рекомендуемая структура вашего решения:

```
lab6/
├── gr<N>sub<M>/
│   └── <ваша_фамилия>/
│       ├── src/
│       │   ├── main.c           # Точка входа
│       │   ├── operations.c     # Реализация file operations
│       │   ├── operations.h
│       │   └── utils.c          # Вспомогательные функции
│       ├── Makefile
│       ├── README.md            # Инструкция по сборке
│       └── REPORT.md            # Отчет с результатами
```

## Базовый шаблон для старта

Скопируйте `samples/passthrough_fuse.c` и модифицируйте под ваш вариант:

```bash
# Для задания B или C создайте новый файл на основе примера
cp samples/passthrough_fuse.c src/my_fuse.c

# Измените функции read/write для реализации вашей логики
# Например, для ROT13:
static int my_read(const char *path, char *buf, size_t size, ...) {
    // 1. Прочитать из файла
    // 2. Применить ROT13 к buf
    // 3. Вернуть результат
}
```

## Полезные команды для тестирования

```bash
# Создать много файлов (для IOPS теста)
for i in {1..100}; do echo "file $i" > /tmp/fuse_mount/file_$i.txt; done

# Большой файл (для throughput теста)
dd if=/dev/zero of=/tmp/fuse_mount/bigfile bs=1M count=100

# Измерить скорость чтения
time dd if=/tmp/fuse_mount/bigfile of=/dev/null bs=1M

# Статистика операций (strace)
strace -c cat /tmp/fuse_mount/test.txt

# Мониторинг в реальном времени
watch -n 1 'ls -lh /tmp/fuse_mount/'
```

## Тестовый сценарий

Базовый тест для проверки всех операций:

```bash
#!/bin/bash
# test_fuse.sh

set -e  # Exit on error

MOUNT="/tmp/fuse_mount"

echo "=== Testing FUSE Filesystem ==="

# 1. Создание файла
echo "Test 1: Create file"
echo "Hello" > $MOUNT/test.txt
[ -f $MOUNT/test.txt ] && echo "✓ File created"

# 2. Чтение файла
echo "Test 2: Read file"
CONTENT=$(cat $MOUNT/test.txt)
[ "$CONTENT" = "Hello" ] && echo "✓ Content matches"

# 3. Запись в файл
echo "Test 3: Write to file"
echo "World" >> $MOUNT/test.txt
grep -q "World" $MOUNT/test.txt && echo "✓ Write successful"

# 4. Создание директории
echo "Test 4: Create directory"
mkdir $MOUNT/testdir
[ -d $MOUNT/testdir ] && echo "✓ Directory created"

# 5. Файл в директории
echo "Test 5: File in directory"
echo "Nested" > $MOUNT/testdir/nested.txt
[ -f $MOUNT/testdir/nested.txt ] && echo "✓ Nested file created"

# 6. Список файлов
echo "Test 6: List directory"
ls $MOUNT/ | grep -q "test.txt" && echo "✓ Directory listing works"

# 7. Удаление файла
echo "Test 7: Delete file"
rm $MOUNT/test.txt
[ ! -f $MOUNT/test.txt ] && echo "✓ File deleted"

# 8. Удаление директории
echo "Test 8: Delete directory"
rm $MOUNT/testdir/nested.txt
rmdir $MOUNT/testdir
[ ! -d $MOUNT/testdir ] && echo "✓ Directory deleted"

echo ""
echo "All tests passed! ✓"
```

Сохраните как `test_fuse.sh`, сделайте исполняемым (`chmod +x test_fuse.sh`) и запустите после монтирования FS.

## Следующие шаги

1. **Прочитайте README.md** — полное описание заданий
2. **Изучите пример** — `samples/passthrough_fuse.c`
3. **Выберите вариант** — нечетный или четный номер
4. **Реализуйте задание A** — базовый passthrough
5. **Реализуйте задание B/C** — специфичное для варианта
6. **Проведите тестирование** — benchmarks и графики
7. **Напишите отчет** — REPORT.md

## Ресурсы

- [libfuse documentation](https://libfuse.github.io/doxygen/)
- [FUSE tutorial](https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/)
- [Example: hello.c](https://github.com/libfuse/libfuse/blob/master/example/hello.c)

**Удачи!**
