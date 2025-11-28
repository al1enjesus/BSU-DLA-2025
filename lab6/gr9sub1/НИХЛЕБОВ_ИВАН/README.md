# Лабораторная работа 6: FUSE Filesystems

## Структура проекта

-   `src/task_a.c`: Исходный код для Задания A (Passthrough)
-   `src/task_b.c`: Исходный код для Задания B (ROT13 Encryption)
-   `src/task_c.c`: Исходный код для Задания C (Uppercase)

## Сборка

Для сборки всех трех версий файловой системы выполните:

```bash
make
```

Это создаст три исполняемых файла в директории `build/`: `task_a`, `task_b`, и `task_c`.

Вы также можете собрать каждую версию по отдельности:
```bash
make build/task_a
make build/task_b
make build/task_c
```

## Запуск и проверка

### Общая подготовка
Создайте директорию-источник и точку монтирования:
```bash
mkdir -p /tmp/source /tmp/fuse
```
**Важно:** Перед запуском новой версии ФС убедитесь, что точка монтирования свободна. Используйте `fusermount -u /tmp/fuse` для размонтирования.

---

### Задание A: Passthrough Filesystem

**Запуск:**
```bash
./build/task_a /tmp/source /tmp/fuse -f
```
Ключ `-f` оставляет программу работать в foreground режиме для удобства отладки и просмотра логов.

**Проверка:**
Откройте новый терминал и выполните стандартные файловые операции. Все они будут проксироваться и логироваться.
```bash
echo "Hello from Task A" > /tmp/fuse/file_a.txt
cat /tmp/fuse/file_a.txt
ls -l /tmp/fuse
rm /tmp/fuse/file_a.txt
```

---

### Задание B: ROT13 Encryption Filesystem

**Запуск:**
```bash
./build/task_b /tmp/source /tmp/fuse -f
```

**Проверка:**
```bash
# Записываем обычный текст
echo "Hello ROT13" > /tmp/fuse/file_b.txt

# Проверяем, что через FUSE он читается как обычный
cat /tmp/fuse/file_b.txt
# Ожидаемый вывод: Hello ROT13

# Проверяем, что на диске он хранится в зашифрованном виде
cat /tmp/source/file_b.txt
# Ожидаемый вывод: Uryyb EBG13
```

---

### Задание C: Uppercase Filesystem

**Запуск:**
```bash
./build/task_c /tmp/source /tmp/fuse -f
```

**Проверка:**
```bash
# Записываем текст в нижнем регистре
echo "Hello Uppercase" > /tmp/fuse/file_c.txt

# Проверяем, что через FUSE он читается в верхнем регистре
cat /tmp/fuse/file_c.txt
# Ожидаемый вывод: HELLO UPPERCASE

# Проверяем, что на диске он хранится в оригинальном виде
cat /tmp/source/file_c.txt
# Ожидаемый вывод: Hello Uppercase
```

## Остановка

Для размонтирования файловой системы нажмите `Ctrl+C` в терминале, где запущен `myfuse`, или выполните команду:

```bash
fusermount -u /tmp/fuse
```