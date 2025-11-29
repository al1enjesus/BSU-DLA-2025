Конечно! Вот переписанная версия отчёта по FUSE в формате Markdown, без упоминаний других авторов, чистая и готовая для вставки:
# Лабораторная работа №6: FUSE Filesystems

**Студент:** Кветко Матвей  
**Группа:** 9, Подгруппа 2

## Структура проекта

- `src/task_a.c` — исходный код Задания A (Passthrough)  
- `src/task_b.c` — исходный код Задания B (ROT13 Encryption)  
- `src/task_c.c` — исходный код Задания C (Uppercase)  

## Сборка

Для сборки всех трех файловых систем выполните:

```bash
make
````

В результате будут созданы три исполняемых файла в директории `build/`: `task_a`, `task_b`, `task_c`.

Можно собрать отдельную версию:

```bash
make build/task_a
make build/task_b
make build/task_c
```

## Запуск и проверка

### Общая подготовка

Создайте директории для исходных файлов и точку монтирования:

```bash
mkdir -p /tmp/source /tmp/fuse
```

**Важно:** Перед запуском новой версии убедитесь, что точка монтирования свободна:

```bash
fusermount -u /tmp/fuse
```

---

### Задание A: Passthrough Filesystem

**Запуск:**

```bash
./build/task_a /tmp/source /tmp/fuse -f
```

Флаг `-f` оставляет программу в foreground для удобства отладки и просмотра логов.

**Проверка:**

```bash
echo "Hello from Task A" > /tmp/fuse/file_a.txt
cat /tmp/fuse/file_a.txt
ls -l /tmp/fuse
rm /tmp/fuse/file_a.txt
```

Все операции проксируются через FUSE и логируются.

---

### Задание B: ROT13 Encryption Filesystem

**Запуск:**

```bash
./build/task_b /tmp/source /tmp/fuse -f
```

**Проверка:**

```bash
# Записываем текст
echo "Hello ROT13" > /tmp/fuse/file_b.txt

# Чтение через FUSE — расшифровка
cat /tmp/fuse/file_b.txt
# Ожидаемый вывод: Hello ROT13

# Проверка на диске — зашифрованное хранение
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

# Чтение через FUSE — преобразование в верхний регистр
cat /tmp/fuse/file_c.txt
# Ожидаемый вывод: HELLO UPPERCASE

# Проверка на диске — оригинальный текст
cat /tmp/source/file_c.txt
# Ожидаемый вывод: Hello Uppercase
```

---

## Остановка

Для размонтирования файловой системы:

* Нажмите `Ctrl+C` в терминале, где запущена FUSE FS
  или выполните:

```bash
fusermount -u /tmp/fuse
```

