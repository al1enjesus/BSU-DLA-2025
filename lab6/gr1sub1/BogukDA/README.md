# Лабораторная работа 6: FUSE Filesystem

Passthrough FUSE файловая система для лабораторной работы.

## Сборка

```bash
make
```

# Общая подготовка перед заданиями

Сoздаем директорию
```bash
mkdir -p /tmp/source /mnt/fuse
```
Для размонтирования использовать команду

```bash
fusermount -u /mnt/fuse
```
# Задание A: Passthrough Filesystem
В одном терминале сделать запуск FUSE
```bash
./passthrough /tmp/source /mnt/fuse -f
```
В другом терминале тестируем команды
```bash
#1. Создание и запись в файл
echo "test" > /mnt/fuse/file.txt
# 2. Чтение файла
cat /mnt/fuse/file.txt
# 3. Просмотр содержимого
ls -la /mnt/fuse/
# 4. Удалениие файла
rm /mnt/fuse/file.txt
```
# Задание B: Archive Filesystem (read-only)
Запуск
```bash
./archive /any/file.tar /mnt/archive -f
```
В другом терминале
```bash
# 1. Просмотр содержимого
ls -la /mnt/archive/
# 2. Чтение файла
cat /mnt/archive/data.txt

# 3. Попытка записи (должна завершиться ошибкой)
echo "test" > /mnt/archive/data.txt

# 4. Попытка создания файла (должна завершиться ошибкой)
touch /mnt/archive/newfile.txt

```
# Задание C: Monitoring Filesystem
Запуск
```bash
./monitor /tmp/source /mnt/monitor -f
```
В другом терминале
```bash
# 1. Просмотр содержимого (должен появиться .stats)
ls -la /mnt/monitor/

# 2. Проверка статистики ДО операций
cat /mnt/monitor/.stats

# 3. Выполняем операции для накопления статистики
cat /mnt/monitor/file1.txt
cat /mnt/monitor/newfile.txt
ls -la /mnt/monitor/
echo "More data" >> /mnt/monitor/file1.txt

# 4. Проверяем обновленную статистику
cat /mnt/monitor/.stats
```