# FUSE Filesystem Implementations - Lab 6

Реализация пользовательских файловых систем с использованием FUSE (Filesystem in Userspace).

## Содержание

- passthrough_fs.c - Задание A: Базовая passthrough файловая система
- rot13_fs.c - Задание B (вариант 2): Шифрование ROT13
- uppercase_fs.c - Задание C (вариант 2): Преобразование в uppercase
- Makefile - Сборка всех программ
- test.sh - Автоматическое тестирование

## Требования

### Linux Debian/Ubuntu
```bash
sudo apt-get update
sudo apt-get install libfuse3-dev fuse3 pkg-config
```

## Сборка

```bash
# Собрать все программы
make

# Собрать конкретную программу
make passthrough_fs
make rot13_fs
make uppercase_fs

# Очистка
make clean
```

## Использование

### Общая структура команды

```bash
./<program> <source_dir> <mountpoint> [FUSE options]
```

Опции FUSE:
- `-f` - foreground режим (не демонизировать, видны логи)
- `-d` - debug режим (подробный вывод от libfuse)
- `-s` - single-threaded режим

### Задание A: Passthrough Filesystem

Простая файловая система, которая проксирует все операции в исходную директорию.

```bash
# Подготовка
mkdir -p /tmp/fuse_source /tmp/fuse_mount

# Запуск
./passthrough_fs /tmp/fuse_source /tmp/fuse_mount -f

# В другом терминале:
echo "Hello FUSE" > /tmp/fuse_mount/test.txt
cat /tmp/fuse_mount/test.txt
ls -la /tmp/fuse_mount/

# Размонтирование (в третьем терминале)
fusermount -u /tmp/fuse_mount
```

**Что логируется:**
```
[\<date> \<time>] CREATE: /test.txt (result: 0)
[\<date> \<time>] WRITE: /test.txt (result: 11)
[\<date> \<time>] OPEN: /test.txt (result: 0)
[\<date> \<time>] READ: /test.txt (result: 11)
```

### Задание B: ROT13 Encryption Filesystem

Файловая система с автоматическим шифрованием/дешифрованием ROT13.

```bash
# Запуск
./rot13_fs /tmp/fuse_source /tmp/fuse_mount -f

# Записать (автоматически зашифруется)
echo "Secret Message" > /tmp/fuse_mount/secret.txt

# Прочитать через FUSE (расшифровано)
cat /tmp/fuse_mount/secret.txt
# Secret Message

# Прочитать напрямую из source (зашифровано)
cat /tmp/fuse_source/secret.txt
# Frperg Zrffntr
```

### Задание C: Uppercase Filesystem

Файловая система, преобразующая весь текст в верхний регистр при чтении.

```bash
# Запуск
./uppercase_fs /tmp/fuse_source /tmp/fuse_mount -f

# Записать в нижнем регистре
echo "hello world" > /tmp/fuse_source/test.txt

# Прочитать напрямую (оригинал)
cat /tmp/fuse_source/test.txt
# hello world

# Прочитать через FUSE (uppercase)
cat /tmp/fuse_mount/test.txt
# HELLO WORLD
```

## Автоматическое тестирование

```bash
# Дать права на выполнение
chmod +x test.sh

# Запустить все тесты
./test.sh all

# Запустить конкретный тест
./test.sh passthrough
./test.sh rot13
./test.sh uppercase
./test.sh benchmark
```

Скрипт автоматически:
1. Создаёт тестовые директории и файлы
2. Запускает каждую файловую систему
3. Выполняет операции чтения/записи
4. Проверяет корректность результатов
5. Измеряет производительность
6. Очищает за собой

## Архитектура решений
## Отладка

### Запуск в debug режиме

```bash
./passthrough_fs /tmp/source /tmp/mount -f -d
```

Флаг `-d` включает подробный вывод от libfuse:
```
unique: 1, opcode: LOOKUP (1), nodeid: 1, insize: 52
LOOKUP /test.txt
   NODEID: 2
   unique: 1, success, outsize: 144
unique: 2, opcode: OPEN (14), nodeid: 2, insize: 48
OPEN flags=0x8000
   unique: 2, success, outsize: 32
```

### Проверка монтирования

```bash
mount | grep fuse
df -h | grep fuse
```

### Принудительное размонтирование

```bash
fusermount -uz /tmp/fuse_mount
```

### Если процесс завис

```bash
# Найти PID
ps aux | grep passthrough_fs

# Убить процесс
kill -9 <PID>

# Размонтировать
fusermount -uz /tmp/fuse_mount
```

## Troubleshooting

### Error: "fuse: device not found"

```bash
sudo modprobe fuse
ls -la /dev/fuse
```

### Error: "Permission denied"

```bash
# Проверить права на директории
ls -la /tmp/fuse_source
ls -la /tmp/fuse_mount

# Дать права
chmod 755 /tmp/fuse_source /tmp/fuse_mount
```

### Error: "Transport endpoint is not connected"

Директория осталась смонтированной после краша:

```bash
fusermount -uz /tmp/fuse_mount
```

## Полезные команды для тестирования

```bash
# Создание файлов
echo "test" > file.txt
dd if=/dev/zero of=bigfile bs=1M count=100

# Чтение
cat file.txt
head -n 10 file.txt
tail -n 10 file.txt

# Копирование
cp source.txt dest.txt
rsync -av source/ dest/

# Статистика
stat file.txt
ls -lh file.txt
du -sh directory/

# Benchmark
time dd if=file of=/dev/null bs=4096
time find /mnt/fuse -type f | wc -l
```

