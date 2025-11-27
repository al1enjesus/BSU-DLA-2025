# Лабораторная работа 6 — Файловые системы FUSE

# 1. Цель работы

Изучить архитектуру VFS и FUSE, реализовать FUSE-файловую систему в userspace. Реализовать passthrough FS (задание A) с логированием и дополнительно — ROT13 шифрование текста на лету (задание B, вариант 2).

# 2. Реализованные file operations

Реализованы и протестированы:

* `getattr` — возвращает `struct stat`
* `readdir` — чтение директории
* `open` — проверка возможности открытия
* `read` — чтение (с трансформациями для ROT13/UPPER)
* `write` — запись (с шифрованием при ROT13)
* `create` — создание файла
* `unlink` — удаление файла
* `mkdir` — создание директории
* `rmdir` — удаление директории

Логирование операций выполняется в `stderr` с таймстампом и информацией о результате (количество байт или код ошибки).

# 3. Ход выполнения

## Среда и зависимости

* ОС: Ubuntu (VirtualBox VM)
* Пакеты:

```bash
sudo apt-get update
sudo apt-get install -y libfuse3-dev fuse3 pkg-config build-essential
```
Проверка:

```bash
pkg-config --modversion fuse3
ls -l /dev/fuse
```

## Структура проекта

```
lab6/
├── src/
│   └── myfuse.c          # основной код
├── Makefile
├── README.md
├── test_fuse.sh
└── REPORT.md
```

## Сборка

В каталоге проекта:

```bash
make
```

Пример вывода, полученного в VM:

```bash
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab6/gr9sub2/PopovaDS$ make
gcc -Wall -Wextra -O2 `pkg-config --cflags fuse3` -o myfuse src/myfuse.c `pkg-config --libs fuse3`
```

## Подготовка директорий

Создаём исходную директорию и точку монтирования:

```bash
sudo mkdir -p /tmp/source /tmp/mount
sudo chown $USER:$USER /tmp/source /tmp/mount
```

Содержимое `/tmp/source` (пример):

```bash
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab6/gr9sub2/PopovaDS$ ls -la /tmp/source
total 12
drwxr-xr-x  2 dasha dasha 4096 Nov 26 22:53 .
drwxrwxrwt 20 root  root  4096 Nov 26 22:53 ..
-rw-rw-r--  1 dasha dasha   12 Nov 26 22:53 hello.txt
```

# 4. Запуск и тестирование

## Задание A — Passthrough

### Сервер (терминал 1)

```bash
./myfuse /tmp/source /tmp/mount --mode=passthrough -f
```

Пример `stderr`-логов:

```text
Mounting /tmp/source at /tmp/mount (mode=passthrough)
Ignoring invalid max threads value 4294967295 > max (100000).
[2025-11-26 22:55:42] GETATTR: /.Trash (res=-2)
[2025-11-26 22:55:42] GETATTR: /.Trash-1000 (res=-2)
[2025-11-26 22:56:16] GETATTR: / (res=0)
[2025-11-26 22:56:16] READDIR: / (res=0)
[2025-11-26 22:56:16] GETATTR: /hello.txt (res=0)
[2025-11-26 22:56:27] GETATTR: /hello.txt (res=0)
[2025-11-26 22:56:27] OPEN: /hello.txt (res=0)
[2025-11-26 22:56:27] READ: /hello.txt (res=0) 12 bytes at 0
[2025-11-26 22:56:34] GETATTR: /test.txt (res=-2)
[2025-11-26 22:56:34] CREATE: /test.txt (res=0)
[2025-11-26 22:56:34] GETATTR: /test.txt (res=0)
[2025-11-26 22:56:34] WRITE: /test.txt (res=0) 10 bytes at 0
[2025-11-26 22:56:34] GETATTR: /test.txt (res=0)
```

### Клиент (терминал 2)

```bash
ls -la /tmp/mount
```

Вывод:

```text
total 12
drwxr-xr-x  2 dasha dasha 4096 Nov 26 22:53 .
drwxrwxrwt 20 root  root  4096 Nov 26 22:53 ..
-rw-rw-r--  1 dasha dasha   12 Nov 26 22:53 hello.txt
```

Чтение:

```bash
cat /tmp/mount/hello.txt
```

Вывод:

```text
hello world
```

Создание файла через FUSE:

```bash
echo "FUSE test" > /tmp/mount/test.txt
```

В логах сервера появились операции `CREATE` и `WRITE`.


## Задание B — ROT13 (вариант 2)

### Сервер (терминал 1)

```bash
./myfuse /tmp/source /tmp/mount --mode=rot13 -f
```

Пример логов:

```text
Mounting /tmp/source at /tmp/mount (mode=rot13)
Ignoring invalid max threads value 4294967295 > max (100000).
[2025-11-26 23:02:19] GETATTR: /.Trash (res=-2)
[2025-11-26 23:02:19] GETATTR: /.Trash-1000 (res=-2)
[2025-11-26 23:02:37] GETATTR: /secret.txt (res=-2)
[2025-11-26 23:02:37] CREATE: /secret.txt (res=0)
[2025-11-26 23:02:37] GETATTR: /secret.txt (res=0)
[2025-11-26 23:02:37] WRITE: /secret.txt (res=0) 12 bytes at 0
[2025-11-26 23:03:09] GETATTR: /secret.txt (res=0)
[2025-11-26 23:03:09] OPEN: /secret.txt (res=0)
[2025-11-26 23:03:09] READ: /secret.txt (res=0) 12 bytes at 0
```

### Клиент (запись и проверка)

Записали через точку монтирования:

```bash
echo "Hello World" > /tmp/mount/secret.txt
```

Проверили реальный файл в `/tmp/source` (на диске должен быть ROT13):

```bash
cat /tmp/source/secret.txt
```

Вывод:

```text
Uryyb Jbeyq
```

Проверили через FUSE (декодируется при чтении):

```bash
cat /tmp/mount/secret.txt
```

Вывод:

```text
Hello World
```

# 5. Ответы на контрольные вопросы 

## Архитектура файловых систем

1. **Что такое VFS (Virtual File System) и зачем она нужна?**
   VFS — абстрактный слой в ядре Linux, предоставляющий единый интерфейс для разных файловых систем, чтобы приложения использовали одни и те же системные вызовы.

2. **Объясните разницу между inode, dentry и file descriptor.**
   `inode` — метаданные файла; `dentry` — кеш имен (связывает имя и inode); `file descriptor` — дескриптор открытого файла в процессе (позиция, флаги).

3. **Что хранится в структуре `struct inode`?**
   Режим/права, владелец (UID/GID), размер, временные метки, указатели на данные, счетчик ссылок и др.

4. **Что такое superblock и какую информацию он содержит?**
   Информация о ФС целиком: размер, свободное/занятое пространство, параметры монтирования, идентификатор.

5. **Как работает кеш dentry и зачем он нужен?**
   Dentry-кеш хранит недавно использованные сопоставления имени → inode для ускорения lookup’ов и уменьшения дисковых/метаданных обращений.

## FUSE

6. **Как FUSE взаимодействует с ядром Linux?**
   Ядро пересылает операции через FUSE kernel module в `/dev/fuse`, откуда libfuse читает запросы и вызывает callbacks в userspace-программе.

7. **Опишите путь системного вызова `read()` в FUSE FS.**
   Программа вызывает `read()` → syscall в ядро → VFS → FUSE kernel module → запрос в `/dev/fuse` → libfuse вызывает `read` callback в userspace → программа возвращает данные → ядро возвращает данные приложению.

8. **Почему FUSE работает медленнее нативных kernel FS?**
   Дополнительные context switches и копирование данных между kernel и userspace, overhead протокола FUSE.

9. **Какие преимущества дает разработка FS в userspace?**
   Безопасность (ошибка в userspace не крашит ядро), простота разработки и отладки, доступ к стандартным библиотекам.

10. **Примеры популярных FUSE FS.**
    sshfs, s3fs, rclone (gcs/drive), encfs.

## File operations

11. **Что делает операция `getattr()` и когда она вызывается?**
    Возвращает `struct stat` — вызывается при `stat()`, `ls -l`, и перед многими операциями.

12. **В чем разница между `open()` и `create()`?**
    `open()` открывает существующий файл; `create()` создаёт файл и открывает его.

13. **Почему `read()` принимает параметр `offset`?**
    Позволяет читать из произвольной позиции (поддержка pread semantics).

14. **Как `readdir()` возвращает список файлов?**
    Через callback `filler(buf, name, stat, offset, flags)` в libfuse.

15. **Что должна возвращать `getattr()` для директории?**
    `st_mode` с `S_IFDIR`, правильный `st_nlink` (>=2), размер (обычно 4096) и др. метаданные.

## Метаданные и атрибуты

16. **Что такое атрибуты файла? Примеры.**
    Размер, права, UID/GID, временные метки (atime/mtime/ctime), режим (тип), inode number.

17. **Что означают биты прав доступа (rwxrwxrwx)?**
    Три триады: владелец (rwx), группа (rwx), остальные (rwx).

18. **Разница между mtime, atime и ctime.**
    `mtime` — время модификации содержимого; `atime` — время последнего доступа; `ctime` — время изменения метаданных.

19. **Что такое hard link и как он отличается от symbolic link?**
    Hard link — альтернативное имя для того же inode (несколько имён у одного файла). Symlink — отдельный файл, содержащий путь к другому файлу.

20. **Как FS определяет размер файла?**
    `st_size` в inode указывает количество байт в файле.

## Физическая организация

21. **Что такое блок в файловой системе?**
    Минимальная единица выделения на диске (например, 4KB).

22. **Принцип непрерывного размещения (contiguous allocation).**
    Файл хранится в одном непрерывном диапазоне блоков — быстро для последовательного доступа, но вызывает фрагментацию.

23. **Как работает linked allocation?**
    Каждый блок содержит указатель на следующий — удобно для добавления, плохо для случайного доступа.

24. **Что такое inode-based allocation?**
    Таблица inode содержит указатели на блоки файла (прямые и косвенные), как в ext.

25. **Сравнение ext4 и btrfs.**
    ext4 — простая, стабильная, inode-базированная. btrfs — copy-on-write, snapshot'ы, встроенная RAID логика и расширенные возможности.

## Производительность и оптимизация

26. **Почему большие файлы читаются быстрее, чем много мелких?**
    Меньше вызовов open/read, эффективнее read-ahead и кеширование.

27. **Что такое read-ahead?**
    Предварительное чтение следующего диапазона данных в кеш для ускорения последовательного чтения.

28. **Как page cache ускоряет доступ?**
    Хранит страницы файла в RAM, позволяя обрабатывать read из памяти без диска.

29. **Почему SSD быстрее HDD?**
    Отсутствие механических задержек (seek, rotation), лучшее время доступа и высокая производительность случайного доступа.

30. **Какие параметры влияют на производительность FUSE FS?**
    overhead context switches, размеры буферов, max_background/max_write, эффективность userspace-обработчика, кеширование.

# 6. Выводы

* Реализовано задание A (passthrough) с логированием; проверено создание/чтение/запись/удаление.
* Реализовано задание B (ROT13) — запись через FUSE шифруется ROT13 на диске, чтение расшифровывает.


# 7. Приложения — команды и логи (как были выполнены)

## Сборка

```bash
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab6/gr9sub2/PopovaDS$ make
gcc -Wall -Wextra -O2 `pkg-config --cflags fuse3` -o myfuse src/myfuse.c `pkg-config --libs fuse3`
```

## Подготовка директорий

```bash
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab6/gr9sub2/PopovaDS$ sudo mkdir -p /tmp/source /tmp/mount
```

## Содержимое `/tmp/source`

```bash
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab6/gr9sub2/PopovaDS$ ls -la /tmp/source
total 12
drwxr-xr-x  2 dasha dasha 4096 Nov 26 22:53 .
drwxrwxrwt 20 root  root  4096 Nov 26 22:53 ..
-rw-rw-r--  1 dasha dasha   12 Nov 26 22:53 hello.txt
```

## Passthrough (Task A) — сессия

### Терминал 1 (server)

```text
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab6/gr9sub2/PopovaDS$ ./myfuse /tmp/source /tmp/mount --mode=passthrough -f
Mounting /tmp/source at /tmp/mount (mode=passthrough)
Ignoring invalid max threads value 4294967295 > max (100000).
[2025-11-26 22:55:42] GETATTR: /.Trash (res=-2)
[2025-11-26 22:55:42] GETATTR: /.Trash-1000 (res=-2)
[2025-11-26 22:56:16] GETATTR: / (res=0)
[2025-11-26 22:56:16] READDIR: / (res=0)
[2025-11-26 22:56:16] GETATTR: /hello.txt (res=0)
[2025-11-26 22:56:27] GETATTR: /hello.txt (res=0)
[2025-11-26 22:56:27] OPEN: /hello.txt (res=0)
[2025-11-26 22:56:27] READ: /hello.txt (res=0) 12 bytes at 0
[2025-11-26 22:56:34] GETATTR: /test.txt (res=-2)
[2025-11-26 22:56:34] CREATE: /test.txt (res=0)
[2025-11-26 22:56:34] GETATTR: /test.txt (res=0)
[2025-11-26 22:56:34] WRITE: /test.txt (res=0) 10 bytes at 0
[2025-11-26 22:56:34] GETATTR: /test.txt (res=0)
```

### Терминал 2 (client)

```bash
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab9$ ls -la /tmp/mount
total 12
drwxr-xr-x  2 dasha dasha 4096 Nov 26 22:53 .
drwxrwxrwt 20 root  root  4096 Nov 26 22:53 ..
-rw-rw-r--  1 dasha dasha   12 Nov 26 22:53 hello.txt

dasha@...$ cat /tmp/mount/hello.txt
hello world

dasha@...$ echo "FUSE test" > /tmp/mount/test.txt
```

## ROT13 (Task B) — сессия

### Запись и проверка (client)

```bash
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab9$ echo "Hello World" > /tmp/mount/secret.txt
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab9$ cat /tmp/source/secret.txt
Uryyb Jbeyq
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab9$ cat /tmp/mount/secret.txt
Hello World
```

### Сервер (логи)

```text
dasha@dasha-VirtualBox:~/Desktop/BSU-DLA-2025/lab9$ ./myfuse /tmp/source /tmp/mount --mode=rot13 -f
Mounting /tmp/source at /tmp/mount (mode=rot13)
Ignoring invalid max threads value 4294967295 > max (100000).
[2025-11-26 23:02:19] GETATTR: /.Trash (res=-2)
[2025-11-26 23:02:19] GETATTR: /.Trash-1000 (res=-2)
[2025-11-26 23:02:37] GETATTR: /secret.txt (res=-2)
[2025-11-26 23:02:37] CREATE: /secret.txt (res=0)
[2025-11-26 23:02:37] GETATTR: /secret.txt (res=0)
[2025-11-26 23:02:37] WRITE: /secret.txt (res=0) 12 bytes at 0
[2025-11-26 23:03:09] GETATTR: /secret.txt (res=0)
[2025-11-26 23:03:09] OPEN: /secret.txt (res=0)
[2025-11-26 23:03:09] READ: /secret.txt (res=0) 12 bytes at 0
```

