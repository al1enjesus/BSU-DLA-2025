# Лабораторная работа 6: FUSE файловые системы

Студент: Кухаревич Александр
Группа: gr1sub1
Вариант: 1 (нечетный)

## Сборка

Установите зависимости:
```bash
sudo apt-get install libfuse3-dev libarchive-dev pkg-config build-essential
```

Сборка:
```bash
cd src
make
```

Это создаст исполняемые файлы в ../bin/

## Запуск

### Задание A: Passthrough FS
```bash
mkdir -p /tmp/source /tmp/mount
echo "test" > /tmp/source/file.txt
../bin/passthrough /tmp/source /tmp/mount -f
# В другом терминале:
ls /tmp/mount
cat /tmp/mount/file.txt
# Размонтирование: fusermount -u /tmp/mount
```

### Задание B: Archive FS
```bash
# Создать tar
tar -cf /tmp/test.tar /tmp/source
mkdir -p /tmp/mount_archive
../bin/archive /tmp/test.tar /tmp/mount_archive -f
# В другом терминале:
ls /tmp/mount_archive
cat /tmp/mount_archive/file.txt
```

### Задание C: Monitoring FS
```bash
mkdir -p /tmp/source /tmp/mount_monitor
../bin/monitoring /tmp/source /tmp/mount_monitor -f
# В другом терминале:
echo "test" > /tmp/mount_monitor/file.txt
cat /tmp/mount_monitor/.stats
```

## Структура проекта
- src/ - исходные коды
- bin/ - исполняемые файлы
- REPORT.md - отчет с результатами