# Лабораторная работа 6: Файловые системы FUSE

## Цель работы

Изучить архитектуру виртуальной файловой системы (VFS) Linux и научиться создавать пользовательские файловые системы с использованием FUSE (Filesystem in Userspace). Понять принципы работы файловых операций, взаимодействие между ядром и userspace, а также оценить производительность userspace файловых систем.

## Теория

### Virtual File System (VFS)

VFS — это абстрактный слой в ядре Linux, который предоставляет единый интерфейс для работы с различными файловыми системами (ext4, btrfs, NFS, FUSE и т.д.). VFS позволяет приложениям использовать одни и те же системные вызовы (`open()`, `read()`, `write()`) независимо от типа файловой системы.

**Ключевые структуры VFS:**
- **superblock** — описывает файловую систему в целом (размер блока, root inode)
- **inode** — метаданные файла (размер, права, временные метки)
- **dentry** — кеш имен директорий (связывает имя файла с inode)
- **file** — открытый файл (file descriptor, позиция чтения)

### FUSE (Filesystem in Userspace)

FUSE позволяет создавать файловые системы в userspace без написания кода для ядра. Драйвер FUSE в ядре перенаправляет VFS операции в userspace процесс через специальный протокол.

**Архитектура:**
```
Приложение (ls, cat)
       ↓
   VFS (ядро)
       ↓
  FUSE kernel module
       ↓ (протокол FUSE)
  /dev/fuse
       ↓
  libfuse (userspace)
       ↓
  Ваша программа
```

**Преимущества FUSE:**
- Разработка в userspace (проще отладка, нет риска kernel panic)
- Можно использовать обычные библиотеки (libcurl, libarchive)
- Безопасность (ошибка не роняет систему)

**Недостатки:**
- Overhead на context switch между kernel и userspace (медленнее на 20-40%)
- Дополнительное копирование данных через kernel/userspace boundary

### Основные file operations

Ваша FUSE программа должна реализовать следующие операции:

```c
struct fuse_operations {
    int (*getattr)(const char *path, struct stat *stbuf);  // stat файла
    int (*readdir)(const char *path, void *buf, ...);      // чтение директории
    int (*open)(const char *path, struct fuse_file_info *fi);
    int (*read)(const char *path, char *buf, size_t size, off_t offset, ...);
    int (*write)(const char *path, const char *buf, size_t size, off_t offset, ...);
    int (*create)(const char *path, mode_t mode, ...);
    int (*unlink)(const char *path);                       // удаление файла
    int (*mkdir)(const char *path, mode_t mode);
    int (*rmdir)(const char *path);
};
```

## Как это работает: bash команды и FUSE

### Путь команды через систему

Когда вы выполняете обычную bash команду, например `cat /mnt/fuse/test.txt`, происходит следующая цепочка вызовов:

```
1. Bash команда "cat"
   ↓
2. cat вызывает open("/mnt/fuse/test.txt")
   ↓
3. Системный вызов (syscall) попадает в ядро Linux
   ↓
4. VFS (Virtual File System) в ядре видит путь /mnt/fuse/
   ↓
5. VFS проверяет: какая ФС смонтирована в /mnt/fuse?
   ↓
6. VFS находит: "О! Это FUSE filesystem"
   ↓
7. FUSE драйвер в ядре перенаправляет запрос через /dev/fuse
   ↓
8. libfuse в userspace получает запрос
   ↓
9. libfuse вызывает вашу функцию passthrough_open()
   ↓
10. Ваш код выполняет реальный open(/tmp/source/test.txt)
   ↓
11. Результат возвращается обратно по цепочке
   ↓
12. cat получает file descriptor и читает данные
```

**Ключевая идея:** Команды `cat`, `ls`, `echo` **не знают**, что работают с FUSE! Они думают, что это обычная файловая система. VFS обеспечивает единый интерфейс для всех типов файловых систем.

### Наглядный пример с логами

Давайте посмотрим, что происходит при выполнении команды записи:

```bash
# Терминал 1: Запускаем FUSE с логированием
./myfuse /tmp/source /mnt/fuse -f

# Терминал 2: Выполняем команду
echo "Hello FUSE" > /mnt/fuse/test.txt
```

**Что увидит ваша FUSE программа в логах:**

```
[2025-01-15 10:30:42] GETATTR: /test.txt (result: -2)    ← проверка: существует ли файл?
[2025-01-15 10:30:42] CREATE: /test.txt (result: 0)      ← создание файла
[2025-01-15 10:30:42] GETATTR: /test.txt (result: 0)     ← снова проверка метаданных
[2025-01-15 10:30:42] OPEN: /test.txt (result: 0)        ← открытие для записи
[2025-01-15 10:30:42] WRITE: /test.txt (11 bytes at offset 0, result: 11)  ← запись данных
```

Одна простая команда `echo` вызывает целую последовательность ваших функций!

### Визуализация работы passthrough

```bash
# Реальная структура на диске:
/tmp/source/
├── file1.txt  (содержит "Hello")
└── dir1/
    └── file2.txt

# После монтирования:
./myfuse /tmp/source /mnt/fuse -f

# Теперь эти команды работают через вашу FUSE FS:
ls /mnt/fuse/           # ← вызывает вашу readdir()
cat /mnt/fuse/file1.txt # ← вызывает вашу read()
```

**Что происходит внутри вашего `passthrough_read()`:**

```c
static int passthrough_read(const char *path, char *buf, size_t size, ...) {
    // path = "/file1.txt" (получен от VFS)

    char fullpath[1024];
    get_full_path(fullpath, path);
    // fullpath = "/tmp/source/file1.txt" (построили полный путь)

    int fd = open(fullpath, O_RDONLY);  // ← читаем из РЕАЛЬНОГО файла
    int res = pread(fd, buf, size, offset);
    close(fd);

    return res;  // ← возвращаем данные обратно в cat
}
```

### Пример с шифрованием (Вариант 2, Задание B)

Вот где становится интересно! Вы можете модифицировать данные "на лету":

```bash
# Записываем через FUSE:
echo "Hello" > /mnt/fuse/secret.txt

# Ваша FUSE программа в write():
passthrough_write() {
    // Получаем от VFS: buf = "Hello"
    rot13(buf);  // ← Преобразуем в "Uryyb"
    // Записываем "Uryyb" в реальный файл /tmp/source/secret.txt
}

# Теперь на диске лежит ЗАШИФРОВАННЫЙ текст:
cat /tmp/source/secret.txt
# Uryyb  ← зашифровано!

# Но через FUSE видим оригинал:
cat /mnt/fuse/secret.txt
# Hello  ← расшифровано автоматически!

# Ваша FUSE программа в read():
passthrough_read() {
    // Читаем "Uryyb" с диска
    rot13(buf);  // ← Преобразуем обратно в "Hello"
    // Возвращаем "Hello" в cat
}
```

### Почему bash команды не знают о FUSE?

Все программы в Linux используют **стандартные системные вызовы**:

```c
// Внутри cat, ls, cp, и ЛЮБОЙ программы:
fd = open("/some/path", O_RDONLY);   // ← универсальный syscall
read(fd, buffer, size);               // ← работает для ВСЕХ ФС
close(fd);
```

**VFS (Virtual File System)** обеспечивает единый интерфейс:

```
Программа → syscall → VFS → конкретная ФС

                      VFS
                       ↓
        ┌──────────────┼──────────────┐
        ↓              ↓              ↓
      ext4          btrfs          FUSE
        ↓              ↓              ↓
      диск          диск       ваш код в userspace!
```

### Детальный разбор: что происходит при `ls -la /mnt/fuse/`?

```bash
ls -la /mnt/fuse/
```

**Последовательность вызовов ваших функций:**

1. **`ls` вызывает `opendir("/mnt/fuse/")`**
   - → VFS → FUSE → ваша `passthrough_open()`

2. **`ls` вызывает `readdir()` в цикле**
   - → VFS → FUSE → ваша `passthrough_readdir()`
   - Вы возвращаете список файлов через `filler(buf, filename, ...)`

3. **Для каждого файла `ls` вызывает `stat()` (для опции `-l`)**
   - → VFS → FUSE → ваша `passthrough_getattr()`
   - Вы возвращаете размер, права доступа, время создания

4. **`ls` форматирует и выводит:**
   ```
   -rw-r--r-- 1 user user 1024 Jan 15 10:30 file1.txt
   drwxr-xr-x 2 user user 4096 Jan 15 10:25 dir1
   ```

### Практический эксперимент с strace

Вы можете отследить все системные вызовы с помощью `strace`:

```bash
# Терминал 1: запустить FUSE
./myfuse /tmp/source /mnt/fuse -f

# Терминал 2: отследить syscalls
strace -e trace=open,read,write,stat cat /mnt/fuse/test.txt
```

**Вывод strace (что видит cat):**
```
openat(AT_FDCWD, "/mnt/fuse/test.txt", O_RDONLY) = 3  ← syscall
fstat(3, {st_mode=S_IFREG|0644, st_size=11, ...}) = 0 ← metadata
read(3, "Hello FUSE\n", 131072)          = 11          ← read data
write(1, "Hello FUSE\n", 11)             = 11          ← вывод в stdout
close(3)                                 = 0
```

**И одновременно в терминале 1 (логи вашей FUSE программы):**
```
[2025-01-15 10:30:45] OPEN: /test.txt (result: 0)
[2025-01-15 10:30:45] GETATTR: /test.txt (result: 0)
[2025-01-15 10:30:45] READ: /test.txt (11 bytes at offset 0, result: 11)
```

### Магия FUSE: что вы можете делать

Команды думают, что читают обычный файл, а вы можете:

- ✅ Читать данные из tar/zip архива (Задание B, вариант 1)
- ✅ Шифровать/расшифровывать на лету (Задание B, вариант 2)
- ✅ Преобразовывать текст (uppercase, Задание C, вариант 2)
- ✅ Собирать статистику обращений (Задание C, вариант 1)
- ✅ Генерировать контент динамически
- ✅ Проксировать запросы в сеть (как sshfs)
- ✅ Делать что угодно!

Всё это **без изменения ни одной bash команды** — они продолжают работать как обычно!

## Задание

### Базовая часть (обязательно для всех)

**Задание A: Passthrough FUSE filesystem**

Реализуйте простую FUSE файловую систему, которая "зеркалирует" существующую директорию. Все операции должны проксироваться в настоящую файловую систему с логированием.

**Требования:**
- Реализовать операции: `getattr`, `readdir`, `open`, `read`, `write`, `create`, `unlink`, `mkdir`, `rmdir`
- Логировать каждую операцию в stderr с форматом: `[TIMESTAMP] OPERATION: path (result)`
- Использовать базовую директорию из аргумента командной строки
- Пример: `./myfuse /tmp/source /mnt/fuse` — монтирует `/tmp/source` в `/mnt/fuse`

**Тестирование:**
```bash
# Создать файл
echo "Hello FUSE" > /mnt/fuse/test.txt

# Прочитать
cat /mnt/fuse/test.txt

# Список файлов
ls -la /mnt/fuse/

# Удалить
rm /mnt/fuse/test.txt
```

### Вариант 1 (нечетные номера в группе)

**Задание B: Archive Filesystem (read-only)**

Создайте read-only FUSE FS, которая позволяет просматривать содержимое tar архива как обычную директорию.

**Требования:**
- Принимать путь к `.tar` файлу при монтировании
- Парсить tar формат (заголовки файлов)
- Реализовать `getattr`, `readdir`, `open`, `read`
- Файлы доступны только для чтения
- Можно использовать `libarchive` или парсить tar формат вручную

**Пример:**
```bash
./archive_fs /path/to/archive.tar /mnt/archive
ls /mnt/archive/         # показывает файлы из архива
cat /mnt/archive/file.txt
```

**Задание C: Monitoring Filesystem**

Реализуйте passthrough FS с подсчетом статистики обращений.

**Требования:**
- Считать количество вызовов каждой операции (read, write, open, etc.)
- Считать количество байт прочитано/записано
- Записывать статистику в файл `/mnt/fuse/.stats` (виртуальный файл)
- Обновлять статистику в реальном времени

**Пример `.stats`:**
```
reads: 142
writes: 38
opens: 95
bytes_read: 524288
bytes_written: 16384
```

### Вариант 2 (четные номера в группе)

**Задание B: ROT13 Encryption Filesystem**

Создайте FUSE FS с простым ROT13 шифрованием текстовых файлов.

**Требования:**
- Все операции чтения/записи проходят через ROT13 преобразование
- Файлы на диске хранятся зашифрованными (ROT13)
- При чтении автоматически расшифровываются
- При записи автоматически шифруются
- ROT13: `A→N, B→O, ..., Z→M` (сдвиг на 13 позиций в алфавите)

**Пример:**
```bash
# Записать (будет зашифровано)
echo "Hello World" > /mnt/fuse/secret.txt

# В реальной FS:
cat /tmp/source/secret.txt
# Uryyb Jbeyq

# Через FUSE (расшифровано):
cat /mnt/fuse/secret.txt
# Hello World
```

**Задание C: Uppercase Filesystem**

Реализуйте FUSE FS, которая преобразует всё содержимое файлов в верхний регистр при чтении.

**Требования:**
- Файлы на диске хранятся в оригинальном виде
- При чтении через FUSE все символы преобразуются в uppercase
- Запись работает обычным образом
- Только чтение модифицируется

**Пример:**
```bash
echo "hello world" > /tmp/source/test.txt

cat /tmp/source/test.txt
# hello world

cat /mnt/fuse/test.txt
# HELLO WORLD
```

## Требования к реализации

### Обязательные требования

1. **Язык:** C или C++
2. **Библиотека:** libfuse3 (или libfuse2)
3. **Сборка:** Makefile с целями `all`, `clean`
4. **Обработка ошибок:** Все операции должны корректно возвращать коды ошибок (-ENOENT, -EACCES, etc.)
5. **Безопасность:** Проверять права доступа, не допускать path traversal атак
6. **Документация:** Комментарии в коде на каждую функцию

### Структура кода

```
lab6/
├── src/
│   ├── myfuse.c          # Основной файл
│   ├── operations.c      # Реализация file operations
│   ├── operations.h
│   └── utils.c           # Вспомогательные функции
├── Makefile
├── README.md             # Инструкция по сборке и запуску
└── REPORT.md             # Отчет
```

## Что должно быть в отчете (REPORT.md)

### 1. Цель работы
Кратко сформулируйте цель лабораторной работы.

### 2. Теоретическая часть
- Объясните архитектуру VFS и роль FUSE
- Нарисуйте схему взаимодействия: приложение → VFS → FUSE kernel → userspace
- Опишите реализованные file operations

### 3. Ход выполнения

#### Задание A
```markdown
## Задание A: Passthrough FUSE

### Реализация
Описание подхода к реализации, ключевые функции.

### Команды для запуска
```bash
make
mkdir -p /tmp/source /mnt/fuse
./myfuse /tmp/source /mnt/fuse
```

### Тестирование
Примеры команд и их вывод:
```bash
$ echo "test" > /mnt/fuse/file.txt
[2025-01-15 10:30:42] CREATE: /file.txt (success)
[2025-01-15 10:30:42] WRITE: /file.txt, 5 bytes (success)

$ cat /mnt/fuse/file.txt
[2025-01-15 10:30:45] OPEN: /file.txt (success)
[2025-01-15 10:30:45] READ: /file.txt, 5 bytes (success)
test
```
```

Аналогично для заданий B и C.

### 4. Нагрузочное тестирование

Проведите benchmarking и постройте графики:

#### Тест 1: Latency операций
Измерьте время выполнения базовых операций:
- `open()` — открытие файла
- `read()` — чтение 4KB
- `write()` — запись 4KB
- `getattr()` — получение метаданных

Сравните с обычной ФС (ext4, tmpfs).

**Методика:**
```bash
# Используйте time или напишите свой бенчмарк
time dd if=/mnt/fuse/test bs=4096 count=1000 of=/dev/null
time dd if=/tmp/source/test bs=4096 count=1000 of=/dev/null
```

#### Тест 2: Throughput
Измерьте скорость последовательного чтения/записи для файлов разного размера (1MB, 10MB, 100MB).

```bash
dd if=/dev/zero of=/mnt/fuse/bigfile bs=1M count=100
```

#### Тест 3: IOPS (операций в секунду)
Создайте много маленьких файлов (1000 файлов по 1KB) и измерьте время.

#### Графики
Постройте графики:
1. Latency (ms) vs тип операции (open, read, write, getattr)
2. Throughput (MB/s) vs размер файла
3. IOPS vs количество файлов
4. Сравнение FUSE FS vs обычная FS

**Что выявляет тестирование:**
- **Overhead от FUSE:** Context switch между kernel и userspace добавляет 20-40% задержки
- **Bottleneck на мелких операциях:** FUSE хуже на большом количестве мелких файлов
- **Приемлемо для больших файлов:** Для больших последовательных чтений overhead меньше

### 5. Ответы на контрольные вопросы
Ответьте на все 30 вопросов из секции ниже.

### 6. Выводы
Подведите итоги работы, опишите сложности и интересные находки.

### 7. Использование AI
Укажите, использовали ли вы AI инструменты и как именно.

## Контрольные вопросы

### Архитектура файловых систем
1. Что такое VFS (Virtual File System) и зачем она нужна?
2. Объясните разницу между inode, dentry и file descriptor.
3. Что хранится в структуре `struct inode`?
4. Что такое superblock и какую информацию он содержит?
5. Как работает кеш dentry и зачем он нужен?

### FUSE
6. Как FUSE взаимодействует с ядром Linux?
7. Опишите путь системного вызова `read()` в FUSE FS.
8. Почему FUSE работает медленнее нативных kernel FS?
9. Какие преимущества дает разработка FS в userspace?
10. Приведите примеры популярных FUSE файловых систем (sshfs, GCS FUSE, etc.).

### File operations
11. Что делает операция `getattr()` и когда она вызывается?
12. В чем разница между `open()` и `create()`?
13. Почему `read()` принимает параметр `offset`?
14. Как `readdir()` возвращает список файлов?
15. Что должна возвращать `getattr()` для директории?

### Метаданные и атрибуты
16. Что такое атрибуты файла? Приведите примеры.
17. Что означают биты прав доступа (rwxrwxrwx)?
18. Объясните разницу между mtime, atime и ctime.
19. Что такое hard link и как он отличается от symbolic link?
20. Как FS определяет размер файла?

### Физическая организация
21. Что такое блок в файловой системе?
22. Объясните принцип непрерывного размещения файлов (contiguous allocation).
23. Как работает организация через связанный список блоков (linked allocation)?
24. Что такое индексный узел (inode-based allocation)?
25. Сравните ext4 и btrfs по структуре хранения данных.

### Производительность и оптимизация
26. Почему большие файлы читаются быстрее, чем много мелких?
27. Что такое read-ahead и как он улучшает производительность?
28. Как page cache ускоряет доступ к файлам?
29. Почему запись на SSD быстрее, чем на HDD?
30. Какие параметры влияют на производительность FUSE FS?

## Этапы защиты лабораторной

1. **Показать отчет** (REPORT.md) с выполненными заданиями и графиками
2. **Демонстрация работы:**
   - Скомпилировать программу
   - Смонтировать FUSE FS
   - Показать выполнение базовых операций (создание, чтение, запись, удаление)
   - Показать специфичные для варианта функции
   - Размонтировать FS
3. **Ответить на контрольные вопросы** (выбираются случайно 5-7 вопросов)

## Полезные ресурсы

### Документация
- [libfuse documentation](https://libfuse.github.io/doxygen/)
- [FUSE Wiki](https://github.com/libfuse/libfuse/wiki)
- [Writing a FUSE Filesystem](https://www.cs.nmsu.edu/~pfeiffer/fuse-tutorial/)

### Примеры
- [hello.c](https://github.com/libfuse/libfuse/blob/master/example/hello.c) — минимальный пример
- [passthrough.c](https://github.com/libfuse/libfuse/blob/master/example/passthrough.c) — полный passthrough FS

### Man pages
```bash
man fuse
man 2 open
man 2 stat
man 2 read
```

## Установка libfuse

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install libfuse3-dev fuse3
```

### macOS
```bash
brew install macfuse
brew install libfuse
```

### Проверка
```bash
pkg-config --modversion fuse3
ls /dev/fuse
```

## Советы по отладке

1. **Используйте флаг `-f`** для запуска в foreground режиме
2. **Добавьте `-d`** для debug вывода от libfuse
3. **Логирование:** Пишите в stderr, не используйте printf (может конфликтовать с FUSE)
4. **Размонтирование:** `fusermount -u /mnt/fuse` или `umount /mnt/fuse`
5. **Если зависло:** `fusermount -uz /mnt/fuse` (force unmount)

## Критерии оценки

- **Задание A (обязательно):** 40%
- **Задание B/C (вариант):** 40%
- **Отчет + тестирование:** 15%
- **Ответы на вопросы:** 5%

**Минимум для зачета:** Выполнено задание A + корректный отчет.

---

**Удачи в разработке вашей первой файловой системы!**
