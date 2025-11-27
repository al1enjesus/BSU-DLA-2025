# FUSE Filesystem Lab

## Описание
Реализация трех FUSE файловых систем:
- **Task A**: Passthrough FS с логированием
- **Task B**: Read-only Archive FS для tar файлов
- **Task C**: Monitoring FS со статистикой

## Требования
- Ubuntu 22.04
- libfuse3-dev
- libarchive-dev
- gcc/g++

## Установка зависимостей
```bash
sudo apt update
sudo apt install -y libfuse3-dev fuse3 libarchive-dev build-essential
```

## Сборка
```bash
make all
```

## Запуск

### Запустить скрипт
```bash
sh test_all.sh
```

### Task A: Passthrough FS
```bash
mkdir -p /tmp/source /tmp/mnt
./build/passthrough_fs /tmp/source /tmp/mnt
# В другом терминале:
echo "Hello FUSE" > /tmp/mnt/test.txt
cat /tmp/mnt/test.txt
# Размонтирование:
fusermount3 -u /tmp/mnt
```

### Task B: Archive FS
```bash
# Создать тестовый архив
mkdir -p /tmp/test_archive
echo "File 1" > /tmp/test_archive/file1.txt
echo "File 2" > /tmp/test_archive/file2.txt
tar -cf /tmp/test.tar -C /tmp/test_archive .

mkdir -p /tmp/archive_mnt
./build/archive_fs /tmp/test.tar /tmp/archive_mnt
# Просмотр:
ls -la /tmp/archive_mnt
cat /tmp/archive_mnt/file1.txt
# Размонтирование:
fusermount3 -u /tmp/archive_mnt
```

### Task C: Monitoring FS
```bash
mkdir -p /tmp/mon_source /tmp/mon_mnt
./build/monitoring_fs /tmp/mon_source /tmp/mon_mnt
# Операции:
echo "test" > /tmp/mon_mnt/file.txt
cat /tmp/mon_mnt/file.txt
cat /tmp/mon_mnt/.stats
# Размонтирование:
fusermount3 -u /tmp/mon_mnt
```

## Очистка
```bash
make clean
```
