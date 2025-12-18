#Лабораторная работа 6 — Файловые системы FUSE

##Цель: Реализация passthrough FUSE (Filesystem in Userspace) файловой системы, которая зеркалирует существующую директорию и проксирует все операции в настоящую файловую систему с логированием.

##Задачи:

Реализовать базовые файловые операции (getattr, readdir, open, read, write, create, unlink, mkdir, rmdir)
Организовать логирование всех операций в stderr
Обеспечить безопасность (проверка path traversal атак)
Реализовать обработку ошибок в соответствии с POSIX
Протестировать работоспособность и производительность системы

##Теоретическая часть
Архитектура VFS и роль FUSE

VFS (Virtual File System) - абстракция в ядре Linux, которая предоставляет единый интерфейс для работы с различными файловыми системами (ext4, NTFS, FAT и др.). VFS определяет стандартный набор операций (file operations), которые должны быть реализованы каждой файловой системой.

FUSE (Filesystem in Userspace) - модуль ядра Linux, который позволяет создавать файловые системы в пользовательском пространстве. Вместо реализации в ядре, разработчик пишет обычную пользовательскую программу, которая общается с FUSE через специальный интерфейс.

Преимущества FUSE:

Безопасность: сбои в файловой системе не приводят к краху ядра
Простота разработки: не требуется знание внутреннего API ядра
Гибкость: возможность создания нетривиальных файловых систем

##Задание A

###Реализованные file operations
Обязательные операции (требовались по заданию):

getattr - получение атрибутов файла
int passthrough_getattr(const char *path, struct stat *stbuf);

Вызывает lstat() на соответствующем файле в базовой директории
Логирует операцию и результат

readdir - чтение содержимого директории
int passthrough_readdir(const char *path, void *buf, fuse_fill_dir_t filler);

Открывает директорию через opendir()
Для каждой записи вызывает filler-функцию
Закрывает директорию и логирует операцию

open - открытие файла
int passthrough_open(const char *path, struct fuse_file_info *fi);

Открывает файл с флагами из fi->flags
Сохраняет файловый дескриптор в fi->fh для последующих операций

read - чтение из файла
int passthrough_read(const char *path, char *buf, size_t size, off_t offset);

Использует pread() на сохраненном файловом дескрипторе
Логирует количество байт и смещение

write - запись в файл
int passthrough_write(const char *path, const char *buf, size_t size, off_t offset);

Использует pwrite() на сохраненном файловом дескрипторе
Логирует количество байт и смещение

create - создание файла
int passthrough_create(const char *path, mode_t mode, struct fuse_file_info *fi);

Вызывает creat() с указанными правами доступа
Сохраняет дескриптор и логирует операцию

unlink - удаление файла
int passthrough_unlink(const char *path);

Вызывает unlink() на соответствующем файле

mkdir - создание директории
int passthrough_mkdir(const char *path, mode_t mode);

Вызывает mkdir() с указанными правами доступа

rmdir - удаление директории
int passthrough_rmdir(const char *path);

Вызывает rmdir() на соответствующей директории
    
###Быстрый старт
```bash
#Терминал 1: Запуск FUSE

# Переход в директорию проекта
cd taskA

# Сборка проекта (если не собрано)
make

# Создание тестовых директорий
mkdir -p /tmp/source /mnt/fuse

# Запуск FUSE в foreground режиме с логированием
./myfuse /tmp/source /mnt/fuse -f


# В терминале 2 (нфблюдайте логгирование в терминале 1) 
# 1. Создание файла
echo "Hello FUSE" > /mnt/fuse/test.txt

# 2. Чтение файла
cat /mnt/fuse/test.txt


# 3. Листинг директории
ls -la /mnt/fuse/

# 4. Создание директории
mkdir /mnt/fuse/documents

# 5. Копирование файла
cp /etc/hosts /mnt/fuse/documents/

# 6. Переименование
mv /mnt/fuse/test.txt /mnt/fuse/example.txt

# 7. Изменение прав
chmod 644 /mnt/fuse/documents/hosts

# 8. Рекурсивный листинг
ls -laR /mnt/fuse/

# 9. Удаление
rm /mnt/fuse/example.txt
rm /mnt/fuse/documents/hosts
rmdir /mnt/fuse/documents

### Также можно отдельно запустить скрипты для демонстрации и бенчмаркинга
./test_suite.sh
./benchmark.sh 
```

###Выводы
Все обязательные операции реализованы
Результаты запуска бенчмаркинга:
Крупные файлы: практически нет overhead (0-16%)
Мелкие файлы: заметный overhead из-за контекстных переключений
Стабильность: отличная, все тесты проходят

##Задание B

Принимать путь к .tar файлу при монтировании
```bash
// Файл: src/main.c
// Строки 55-63: Парсинг аргументов командной строки
int main(int argc, char *argv[]) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }
    
    // argv[1] = архив, argv[2] = точка монтирования
    char *archive_path = realpath(argv[1], NULL);
    // ... передача archive_path в структуру данных
```
Парсить tar формат (заголовки файлов)
```bash
// Файл: src/archive_ops.c
// Строки 215-257: Основная функция парсинга tar архива
int parse_tar_archive(struct archive_data *data) {
    struct archive *a = archive_read_new();
    archive_read_support_format_tar(a);  // Поддержка tar формата
    archive_read_support_filter_all(a);
    
    // Чтение заголовков файлов
    while (1) {
        r = archive_read_next_header(a, &entry);  // Чтение заголовка
        const char *pathname = archive_entry_pathname(entry);
        off_t size = archive_entry_size(entry);
        // ... обработка каждого файла/директории
```
Реализовать getattr, readdir, open, read
```bash
// getattr - получение атрибутов файла
int archive_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)

// readdir - чтение содержимого директории
int archive_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)

// open - открытие файла
int archive_open(const char *path, struct fuse_file_info *fi)

// read - чтение из файла
int archive_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
```
Файлы доступны только для чтения
```bash
// Проверка в open()
c

// Строки 424-428: archive_open() - проверка режима доступа
if ((fi->flags & O_ACCMODE) != O_RDONLY) {
    pthread_mutex_unlock(&data->mutex);
    return -EACCES;  // Запрет на запись
}

// Проверка в access()
c

// Строки 557-562: archive_access() - проверка прав доступа
if (mask & W_OK) {
    pthread_mutex_unlock(&data->mutex);
    return -EACCES;  // Read-only файловая система
}

// Запрет в statfs()
c

// Строки 532-536: archive_statfs() - пометка ФС как read-only
stbuf->f_flag = ST_RDONLY;  // Флаг только для чтения
stbuf->f_bfree = 0;         // Нет свободного места (только чтение)
stbuf->f_bavail = 0;        // Нет доступного места
```
Использование libarchive
```bash
// Строки 218-223: Инициализация libarchive
struct archive *a = archive_read_new();
archive_read_support_format_tar(a);     // Поддержка tar
archive_read_support_filter_all(a);     // Поддержка сжатия
archive_read_open_fd(a, data->fd, 10240);  // Открытие архива
```

Обработка ошибок: возврат корректных кодов
```bash
return -ENOENT;   // Файл не найден (archive_getattr, строка 312)
return -EACCES;   // Доступ запрещен (archive_open, строка 428)
return -EISDIR;   // Это директория (archive_open, строка 425)
return -EBADF;    // Неверный файловый дескриптор (archive_read, строка 450) 
```
Безопасность: защита от path traversal
```bash
// Строки 17-55: safe_normalize_path() - безопасная нормализация пути
char* safe_normalize_path(const char *path) {
    // Проверка на ".." и другие опасные компоненты
    if (strcmp(token, "..") == 0) {
        return NULL;  // Опасный путь отклоняется
    }
```

### Быстрый старт
```bash
# make          -   Создает исполняемый файл archive_fs
# make test	    -	Проверяет все функции системы
# make demo	    -	Показывает работу файловой системы
# make clean	-	Удаляет скомпилированные файлы

### В терминале 1
# 1. Сборка
make

# 2. Создание тестового архива
mkdir -p demo_data
echo "Hello from Archive FUSE!" > demo_data/welcome.txt
echo "This is a test file" > demo_data/test.txt
mkdir -p demo_data/subdir
echo "Nested content" > demo_data/subdir/nested.txt
tar -cf demo.tar -C demo_data .

# 3. Создание точки монтирования
mkdir -p /tmp/my_archive

# 4. Запуск файловой системы (foreground режим)
./archive_fs demo.tar /tmp/my_archive -f

### В терминале 2
# 1. Просмотр содержимого
ls -la /tmp/my_archive/

# 2. Чтение файлов
cat /tmp/my_archive/welcome.txt
cat /tmp/my_archive/subdir/nested.txt

# 3. Поиск файлов
find /tmp/my_archive -type f

# 4. Проверка прав доступа (должно быть только чтение)
touch /tmp/my_archive/new_file.txt  # Должно выдать ошибку

# 5. Проверка метаданных
stat /tmp/my_archive/welcome.txt

### Запустите демонстрационные скрипты
make demo 
make test
```

### Выводы
Все основные операции работают корректно:

Монтирование архива (test.tar, demo.tar) успешно.
Чтение файлов (file1.txt, subdir/nested.txt, README.txt, code/sample.c) работает, данные выводятся правильно.
Отображение структуры директорий (команда ls) показывает все файлы и поддиректории.

Безопасность:

Защита от path traversal атак: попытка доступа к ../etc/passwd отклоняется (тест 4.4).
Файловая система доступна только для чтения: попытка создания файла завершается ошибкой (тест 4.5).

Обработка ошибок:

При попытке монтирования несуществующего архива выводится понятное сообщение об ошибке (тест 6).
При монтировании битого архива процесс завершается с ошибкой (тест 5).

Стабильность:

Файловая система корректно инициализируется и уничтожается (видно по логам DEBUG: Initializing... и DEBUG: Destroying...).
Нет утечек ресурсов (все файловые дескрипторы закрываются, память освобождается).

Производительность (качественно):

Операции чтения выполняются быстро (задержки в миллисекундах).
Парсинг архива происходит за приемлемое время (для небольшого архива).

## Задание C

Основная структура
```bash
/* Атомарные счетчики статистики (потокобезопасные) */
typedef struct {
    _Atomic unsigned long reads;
    _Atomic unsigned long writes;
    _Atomic unsigned long opens;
    _Atomic unsigned long creates;
    _Atomic unsigned long getattrs;
    _Atomic unsigned long readdirs;
    _Atomic unsigned long bytes_read;
    _Atomic unsigned long bytes_written;
    _Atomic unsigned long stats_file_reads;
} fs_stats_t;

static fs_stats_t stats = {0};  /* Глобальная статистика */
```
Подсчет операций
```bash
/* log_and_update_stat() - Логирует операцию и обновляет счетчики */
static void log_and_update_stat(const char *operation, const char *path, 
                               ssize_t bytes, off_t offset, int result);
// Вызывается из каждой операции FUSE для подсчета
```
 Виртуальный файл .stats
```bash
/* format_stats_string() - Форматирует статистику в строку */
static char* format_stats_string(size_t *out_size);
// Возвращает строку с текущей статистикой для чтения

/* is_stats_file() - Проверяет, запрашивается ли .stats */
static int is_stats_file(const char *path);
// Определяет виртуальный файл для особой обработки
```
Passthrough операции (проксирование):
```bash
/* get_full_path() - Безопасно преобразует путь */
static char* get_full_path(const char *path);
// Проксирует путь из FUSE в исходную директорию с защитой от traversal

/* monitoring_read() - Чтение файла */
static int monitoring_read(const char *path, char *buf, size_t size, off_t offset,
                           struct fuse_file_info *fi);
// Читает данные, обновляет статистику (bytes_read)

/* monitoring_write() - Запись файла */
static int monitoring_write(const char *path, const char *buf, size_t size,
                            off_t offset, struct fuse_file_info *fi);
// Записывает данные, обновляет статистику (bytes_written)

/* monitoring_getattr() - Получение атрибутов */
static int monitoring_getattr(const char *path, struct stat *stbuf,
                              struct fuse_file_info *fi);
// Для .stats возвращает статические атрибуты, для остальных - проксирует

/* monitoring_readdir() - Чтение директории */
static int monitoring_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                              off_t offset, struct fuse_file_info *fi,
                              enum fuse_readdir_flags flags);
// Добавляет .stats в список файлов корневой директории
```
Остальные операции (аналогично)
```bash
monitoring_open()      // Открытие файлов
monitoring_create()    // Создание файлов  
monitoring_unlink()    // Удаление файлов
monitoring_mkdir()     // Создание директорий
monitoring_rmdir()     // Удаление директорий
monitoring_rename()    // Переименование
// Каждая обновляет соответствующие счетчики
```
Структура операций FUSE
```bash
static struct fuse_operations monitoring_oper = {
    .getattr    = monitoring_getattr,    /* getattr - атрибуты */
    .readdir    = monitoring_readdir,    /* readdir - список файлов */
    .open       = monitoring_open,       /* open - открытие */
    .read       = monitoring_read,       /* read - чтение */
    .write      = monitoring_write,      /* write - запись */
    .create     = monitoring_create,     /* create - создание файла */
    .unlink     = monitoring_unlink,     /* unlink - удаление файла */
    .mkdir      = monitoring_mkdir,      /* mkdir - создание директории */
    .rmdir      = monitoring_rmdir,      /* rmdir - удаление директории */
    .rename     = monitoring_rename,     /* rename - переименование */
    .truncate   = monitoring_truncate,   /* truncate - изменение размера */
    .release    = monitoring_release,    /* release - закрытие файла */
    .access     = monitoring_access,     /* access - проверка доступа */
    .statfs     = monitoring_statfs,     /* statfs - статистика ФС */
};
```

```bash
Пользовательская операция (read/write/open)
         ↓
FUSE вызывает соответствующую функцию
         ↓
monitoring_*() обновляет счетчики в stats
         ↓
Проксирование в исходную ФС (через get_full_path())
         ↓
Возврат результата пользователю
```

### Быстрый старт

```bash
### Терминал 1: Запуск файловой системы

# Переходим в директорию проекта
cd taskC

# Компилируем программу
make

# Очищаем предыдущие тесты
fusermount -u /tmp/fuse_mount 2>/dev/null || true
rm -rf /tmp/source /tmp/fuse_mount

# Создаем необходимые директории
mkdir -p /tmp/source /tmp/fuse_mount

# Добавляем тестовый файл в исходную директорию
echo "Hello from source directory" > /tmp/source/test.txt

# Запускаем Monitoring FUSE с логированием операций
./monitoring_fuse /tmp/source /tmp/fuse_mount -f -d

### Терминал 2: Тестирование и демонстрация

# 1. Просмотр точки монтирования
ls -la /tmp/fuse_mount/

# 2. Чтение виртуального файла статистики
cat /tmp/fuse_mount/.stats

# 3. Чтение существующего файла
cat /tmp/fuse_mount/test.txt

# 4. Создание нового файла
echo "Test content" > /tmp/fuse_mount/newfile.txt

# 5. Создание директории
mkdir /tmp/fuse_mount/mydir

# 6. Создание файла в поддиректории
echo "File in subdir" > /tmp/fuse_mount/mydir/subfile.txt

# 7. Список всех файлов
ls -la /tmp/fuse_mount/

# 8. Просмотр обновленной статистики
cat /tmp/fuse_mount/.stats

# 9. Удаление файлов
rm /tmp/fuse_mount/newfile.txt
rm /tmp/fuse_mount/mydir/subfile.txt
rmdir /tmp/fuse_mount/mydir

# 10. Финальная статистика
cat /tmp/fuse_mount/.stats


### Также можете запустить скрипт для демонстрации и бенчмаркинга
./demo.sh 
./benchmark.sh
```

### Выводы

Работоспособность:

Файловая система успешно монтируется и размонтируется.
Все базовые операции (чтение, запись, создание файлов и директорий) работают корректно.
Виртуальный файл статистики .stats создается и доступен для чтения.

В демо-тесте после выполнения набора операций статистика показывает:

getattr (получение атрибутов файлов)
readdir (чтение директории)
open (открытие файлов)
create (создание файлов)
read (чтение файла)
write (запись в файл)
mkdir (создание директории)

Также подсчитываются байты.

Производительность (по benchmark.sh):

Создание 100 файлов: 0.10 секунд
Чтение 100 файлов: 0.10 секунд
Удаление 100 файлов: 0.08 секунд
Создание 50 директорий: 0.06 секунд
Удаление 50 директорий: 0.05 секунд
Смешанные операции (создание/чтение/удаление 50 файлов): 0.11 секунд

В benchmark.sh было выполнено 152 операции создания файлов, 158 чтений, 152 записи, 150 удалений, 50 созданий и удалений директорий.
Общий объем данных: прочитано 1 МБ, записано 2 МБ.
Файловая система справляется с нагрузкой, что видно по времени выполнения операций.

Файловая система корректно обрабатывает ошибки (например, попытка доступа к несуществующему файлу возвращает ошибку ENOENT).
Защита от path traversal реализована.

Режим отладки:
При запуске с флагом -d выводятся логи операций в формате [TIMESTAMP] OPERATION: path (result).

## Ответы на вопросы

1. VFS (Virtual File System)
Абстрактный слой между ядром и конкретными ФС (ext4, NTFS, FUSE)
Единый API для всех ФС, позволяет монтировать разные ФС без перекомпиляции ядра

2. Inode vs Dentry vs File Descriptor
- Inode - метаданные файла (права, размер, время, блоки данных)
- Dentry - запись в кэше путей (связывает имя с inode) 
- FD - дескриптор открытого файла в процессе (смещение, флаги)

3. struct inode хранит
- Размер файла и тип (файл/директория/симлинк)
- Права доступа (rwx)
- Временные метки (atime, mtime, ctime)
- Указатели на блоки данных
- Счетчик ссылок (hard links)

4. Superblock
Метаданные всей ФС 
Содержит размер ФС, количество свободных блоков, список inode, тип ФС, magic number

5. Dentry кэш
Как работает: Кэширует преобразование путей -> inode
Ускоряет доступ к файлам по пути, избегая парсинга пути при каждом обращении

FUSE

1. Взаимодействие FUSE с ядром
Ядро -> FUSE драйвер -> /dev/fuse -> libfuse -> userspace процесс
Все операции передаются в userspace через файловый дескриптор /dev/fuse

2. Путь read() в FUSE
read() -> VFS -> FUSE kernel модуль -> /dev/fuse -> userspace FUSE процесс -> обработка -> возврат данных

3. Почему FUSE медленнее
- Контекстные переключения kernel - userspace
- Дополнительные копии данных
- Нет прямого доступа к кэшу страниц ядра

 4. Преимущества userspace FS
- Безопасность (падение FS не ломает систему)
- Легкая разработка и отладка
- Не требует прав root
- Кроссплатформенность

 5. Примеры FUSE FS
- sshfs - доступ к удаленным файлам по SSH
- gocryptfs - шифрование на лету
- rclone - облачные хранилища
- NTFS-3G - доступ к NTFS в Linux
- ArchiveFS - доступ к архивам как к ФС

File operations

1. getattr()
Получение атрибутов файла (размер, права, время) 
При stat(), ls, open() - проверка существования и прав

2. open() vs create()
- open() - открытие существующего файла
- create() - создание нового файла (O_CREAT)

3. offset в read()
Позволяет читать с произвольной позиции (для многопоточного чтения, докачки)

 4. readdir()
Использует callback-функцию filler() для добавления имен файлов в буфер 
filler(buf, filename, NULL, 0)

5. getattr() для директории
Возвращает st_mode = S_IFDIR | права, st_size обычно 4096 (размер блока)

Метаданные и атрибуты

1. Атрибуты файла
Размер, тип, права доступа (0644), временные метки, владелец (UID/GID), ACL

 2. Биты прав rwxrwxrwx
- Первые 3: owner (user) - владелец
- Следующие 3: group - группа 
- Последние 3: others - все остальные
- Пример: 755 = rwxr-xr-x (владелец: все, остальные: чтение+исполнение)

3. mtime vs atime vs ctime
- mtime - изменение содержимого файла
- atime - последний доступ (чтение)
- ctime - изменение метаданных (права, владелец)

4. Hard link vs Symbolic link
- Hard link - дополнительное имя для того же inode (один файл, несколько имен)
- Symlink - файл с путем к другому файлу (разные inode)

5. Определение размера файла
Из struct inode -> поле st_size (для обычных файлов) или вычисление суммы размеров блоков

Физическая организация

1. Блок в ФС
Минимальная единица выделения на диске (обычно 4KB). Все файлы выделяются целыми блоками.

 2. Contiguous allocation
Файл занимает последовательные блоки на диске. 
+ Быстрый доступ 
- Фрагментация, сложное расширение

3. Linked allocation
Каждый блок содержит указатель на следующий блок. 
+ Нет внешней фрагментации 
- Медленный доступ (надо пройти всю цепочку)

 4. Inode-based allocation
Inode содержит прямые/косвенные указатели на блоки. 
+ Быстрый доступ, поддержка больших файлов 
- Сложнее реализация (ext2/3/4, UFS)

5. ext4 vs btrfs
- ext4 - inode-based, журналируемая, стабильная, но без snapshots, сжатия
- btrfs - copy-on-write, snapshots, RAID, сжатие, проверка целостности

Производительность и оптимизация

1. Большие файлы vs много мелких
- Большие: Последовательное чтение, меньше seek операций
- Мелкие: Много метаданных, случайный доступ, overhead на открытие/закрытие

2. Read-ahead
Предварительная загрузка следующих блоков в кэш до запроса. Уменьшает latency при последовательном чтении.

3. Page cache
Кэширование страниц памяти с содержимым файлов. Повторное чтение -> из RAM, а не с диска.

4. SSD vs HDD
- SSD: Нет механических частей, параллельный доступ к ячейкам, меньше latency
- HDD: Механическое позиционирование головки, seek time ~10ms

5. Параметры производительности FUSE
- max_readahead - предварительное чтение
- max_write - максимальный размер записи за операцию
- direct_io - обход page cache (быстрее для больших файлов)
- async_read - асинхронные операции
- big_writes - объединение мелких записей
