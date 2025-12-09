# Лабораторная работа 6

## Файлы проекта
- `simple_fuse.c` - Passthrough FUSE (Задание A)
- `archive_fs_fixed.c` - Archive Filesystem (Задание B) 
- `monitoring_fs.c` - Monitoring Filesystem (Задание C)

## Установка зависимостей
```bash
sudo apt-get install libfuse3-dev fuse3 build-essential
```

## Сборка
```bash
gcc -o myfuse simple_fuse.c -lfuse3
gcc -o archive_fs_fixed archive_fs_fixed.c -lfuse3
gcc -o monitoring_fs monitoring_fs.c -lfuse3 -lpthread
```

## Быстрый старт

### Passthrough FUSE
```bash
./myfuse /tmp/source ~/mnt/fuse -f
# Проверка: ls ~/mnt/fuse; cat ~/mnt/fuse/file1.txt
```

### Archive Filesystem  
```bash
./archive_fs_fixed test_archive.tar ~/mnt/archive -f
# Проверка: ls ~/mnt/archive; cat ~/mnt/archive/file1.txt
```

### Monitoring Filesystem
```bash
./monitoring_fs /tmp/source ~/mnt/monitor -f  
# Проверка: cat ~/mnt/monitor/.stats
```

## Остановка
```bash
fusermount -u ~/mnt/fuse
fusermount -u ~/mnt/archive
fusermount -u ~/mnt/monitor
```

## Тестирование
```bash
# Создание тестовых данных
mkdir -p /tmp/source
echo "test" > /tmp/source/file1.txt
tar -cf test_archive.tar /tmp/source/file1.txt
```