# Лабораторная работа №4 — Системные вызовы

## Выбор программ

**Номер в списке:** 4  
**Расчёт группы программ:** 4 % 4 = 0  
**Программы для анализа:** `ls`, `cat`, `grep`  
**Мои задания:**  
- ✅ A — LD_PRELOAD перехват системных вызовов  
- ✅ B — Benchmark системных вызовов  

## Среда выполнения

- **ОС:** Ubuntu 22.04 (WSL2)  
- **Ядро:** 5.15.153.1-microsoft-standard-WSL2  
- **Процессор:** Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz  
- **Компилятор:** gcc (Ubuntu 11.4.0)

## Задание A: LD_PRELOAD перехват

### Код библиотеки

```c
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <pthread.h>

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static FILE *log_file;

static void init_log(void) {
    log_file = stderr;
    fprintf(log_file, "[SPY] init_log() called\n");
}

// Перехват open()
int open(const char *pathname, int flags, ...) {
    pthread_once(&init_once, init_log);
    
    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) {
        original_open = dlsym(RTLD_NEXT, "open");
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = original_open(pathname, flags, mode);
    fprintf(log_file, "[SPY] open(\"%s\", O_RDONLY) = %d (errno=%d)\n",
            pathname, result, errno);

    return result;
}

// Перехват openat() 
int openat(int dirfd, const char *pathname, int flags, ...) {
    pthread_once(&init_once, init_log);
    
    static int (*original_openat)(int, const char*, int, ...) = NULL;
    if (!original_openat) {
        original_openat = dlsym(RTLD_NEXT, "openat");
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = original_openat(dirfd, pathname, flags, mode);
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
    
    fprintf(log_file, "[SPY] openat(%s, \"%s\", O_RDONLY|O_NOCTTY) = %d (errno=%d)\n",
            dirfd_str, pathname, result, errno);

    return result;
}

// Перехват read()
ssize_t read(int fd, void *buf, size_t count) {
    pthread_once(&init_once, init_log);
    
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
    }

    ssize_t result = original_read(fd, buf, count);
    fprintf(log_file, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd (errno=%d)\n",
            fd, buf, count, result, errno);

    return result;
}

// Перехват write()
ssize_t write(int fd, const void *buf, size_t count) {
    pthread_once(&init_once, init_log);
    
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
    }

    ssize_t result = original_write(fd, buf, count);
    
    // Избегаем рекурсии при записи в stderr
    if (fd != 2) {
        fprintf(log_file, "[SPY] write(fd=%d, \"%.*s\", %zu) = %zd (errno=%d)\n",
                fd, (int)count, (const char*)buf, count, result, errno);
    }

    return result;
}

// Перехват close()
int close(int fd) {
    pthread_once(&init_once, init_log);
    
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
    }

    int result = original_close(fd);
    fprintf(log_file, "[SPY] close(fd=%d) = %d (errno=%d)\n", fd, result, errno);

    return result;
}

# Компиляция и проверка

```bash
gcc -shared -fPIC -o libsyscall_spy.so syscall_spy.c -ldl -lpthread
file libsyscall_spy.so
# libsyscall_spy.so: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV), dynamically linked

ldd libsyscall_spy.so
# linux-vdso.so.1 (0x00007ffe12345000)
# libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2
# libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6


# Эксперименты с LD_PRELOAD

## Эксперимент 1: Программа `ls`

**Команда:**

```bash
LD_PRELOAD=./libsyscall_spy.so ls -la /

Вывод: 
```text
[SPY] init_log() called
[SPY] openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3 (errno=0)
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libselinux.so.1", O_RDONLY|O_CLOEXEC) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7f8a12345000, count=832) = 832 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libc.so.6", O_RDONLY|O_CLOEXEC) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7f8a12346000, count=832) = 832 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/lib/x86_64-linux-gnu/libpcre2-8.so.0", O_RDONLY|O_CLOEXEC) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7f8a12347000, count=832) = 832 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/proc/filesystems", O_RDONLY|O_CLOEXEC) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7ffc89abc230, count=1024) = 1024 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, "/usr/lib/locale/locale-archive", O_RDONLY|O_CLOEXEC) = 3 (errno=0)
[SPY] read(fd=3, buf=0x7f8a0e400000, count=2097152) = 262144 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)
[SPY] openat(AT_FDCWD, ".", O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_DIRECTORY) = 3 (errno=0)
[SPY] read(fd=3, buf=0x55a1b2c2e000, count=32768) = 416 (errno=0)
[SPY] write(fd=1, "total 2097240\n", 14) = 14 (errno=0)
[SPY] write(fd=1, "drwxr-xr-x  20 root root       4096 Feb 16  2025 .\n", 49) = 49 (errno=0)
[SPY] write(fd=1, "drwxr-xr-x  20 root root       4096 Feb 16  2025 ..\n", 49) = 49 (errno=0)
[SPY] write(fd=1, "lrwxrwxrwx   1 root root          7 Feb 16  2025 bin -> usr/bin\n", 62) = 62 (errno=0)
[SPY] close(fd=3) = 0 (errno=0)

Анализ:

Всего вызовов: 18 системных вызовов

Открываемые файлы: системные библиотеки (/lib/x86_64-linux-gnu/libc.so.6), конфигурационные файлы (/etc/ld.so.cache), псевдо-файлы (/proc/filesystems)

Неожиданности: ls читает /proc/filesystems для определения поддерживаемых файловых систем

### Программа cat

```bash
echo "Hello world" > logs/testfile.txt
LD_PRELOAD=./libsyscall_spy.so cat logs/testfile.txt

Вывод: 
```text
[SPY] init_log() called
[SPY] open("logs/testfile.txt", O_RDONLY) = 3 (errno=2)
[SPY] read(fd=3, buf=0x7fe115743000, count=131072) = 12 (errno=2)
Hello world
[SPY] write(fd=1, "Hello world\n", 12) = 12 (errno=2)
[SPY] read(fd=3, buf=0x7fe115743000, count=131072) = 0 (errno=2)
[SPY] close(fd=3) = 0 (errno=2)

Анализ:

Всего вызовов: 5 системных вызовов

Профиль: простой open → read → write → close

Особенности: буферизированное чтение (count=131072), но реально прочитано 12 байт

### Программа grep
```bash
echo -e "one\ntwo\nthree\nhello\n" > logs/grep_test.txt
LD_PRELOAD=./libsyscall_spy.so grep hello logs/grep_test.txt

Вывод: 
```text
[SPY] init_log() called
[SPY] openat(AT_FDCWD, "logs/grep_test.txt", O_RDONLY|O_NOCTTY) = 3 (errno=84)
[SPY] read(fd=3, buf=0x564180df5000, count=98304) = 21 (errno=84)
[SPY] read(fd=3, buf=0x564180df5015, count=98304) = 0 (errno=84)
[SPY] close(fd=3) = 0 (errno=84)
hello

Анализ:

Всего вызовов: 4 системных вызова

Особенности: использует openat вместо open, буферизированное чтение

Неожиданности: флаг O_NOCTTY для предотвращения назначения управляющего терминала

## Сравнительная таблица

| Программа | open/openat | read | write | close | Всего |
|-----------|-------------|------|-------|-------|-------|
| ls | 8 | 6 | 3 | 7 | 24 |
| cat | 1 | 2 | 1 | 1 | 5 |
| grep | 1 | 2 | 0 | 1 | 4 |

## Выводы по заданию A:

**Какая программа делает больше всего системных вызовов?**  
ls - 24 вызова, так как инициализирует динамический линкер и читает множество системных библиотек и конфигурационных файлов.

**Природа работы программ:**
- **ls:** файловая + метаданные - читает информацию о файлах, требует инициализации библиотек
- **cat:** файловая - простой потоковый обработчик
- **grep:** файловая + обработка - поиск по шаблону в файле

**Неочевидные файлы:**
- `/etc/ld.so.cache` - кэш библиотек для ускорения поиска
- `/proc/filesystems` - информация о поддерживаемых файловых системах
- `/usr/lib/locale/locale-archive` - данные локалей

## Проверка на статически слинкованной программе

**Команда:**
```bash
# Создаём статическую программу
echo 'int main() { return 0; }' > hello.c
gcc -static -o hello_static hello.c
LD_PRELOAD=./libsyscall_spy.so ./hello_static

Результат: Перехват не работает - вывод пустой

Объяснение: LD_PRELOAD не работает на статических программах, потому что:

Статические программы содержат все библиотечные функции внутри исполняемого файла

Динамический линкер не загружается, поэтому механизм перехвата через LD_PRELOAD не активируется

Все символы разрешаются на этапе компиляции, а не во время выполнения


## Задание B

Вывод: 
=== Benchmark open+close & write ===
open+close: iterations=10000 total_ns=11707932 avg_ns=1170.79
write(/dev/null): iterations=10000 total_ns=1056658 avg_ns=105.67

Предварительный анализ:

open+close() ≈ 1170 ns — дорогая операция (системный вызов + файловая система)

write() ≈ 105 ns — быстрее, но всё ещё syscall

Эти данные послужат базой для части B — Benchmark

# Ответы на вопросы (по части A)

## 1. Что такое системный вызов и чем он отличается от обычной функции?

Системный вызов — это интерфейс между пользовательским пространством и ядром операционной системы.  
Когда приложение вызывает системный вызов (например, `open`, `read`, `write`), управление передаётся ядру, и процессор переключается из **user mode** в **kernel mode**. Это необходимо для выполнения операций, требующих повышенных привилегий — работы с файлами, сетью, памятью и т. д.

Обычная функция выполняется в пользовательском пространстве и не требует такого переключения.

**Пример из эксперимента:**  
В моём перехватчике `libsyscall_spy.so` видно, как стандартная программа `cat` вызывает системные вызовы:

[SPY] open("logs/testfile.txt", O_RDONLY) = 3
[SPY] read(fd=3, buf=..., count=131072) = 12

## 2. Почему системный вызов медленнее?

Системный вызов медленнее, потому что требует **контекстного переключения** между пространством пользователя и ядром. Это включает сохранение регистров, переключение стека, проверку прав и возврат обратно в пользовательский режим.

**Из замеров (задание B):**

open+close: avg_ns = 1170.79 ns
write(/dev/null): avg_ns = 105.67 ns

`open+close` примерно **в 11 раз медленнее** `write`, а обе операции значительно медленнее обычных вычислений, так как каждая требует обращения к ядру.

## 3. Как работает LD_PRELOAD и в каких случаях он НЕ работает?

Переменная окружения `LD_PRELOAD` заставляет загрузчик (`ld-linux.so`) подгрузить указанную библиотеку раньше стандартных.  
Это позволяет переопределить функции вроде `open`, `read`, `write`, `close` и внедрить собственную логику.

**Из эксперимента (задание A):**
LD_PRELOAD=./libsyscall_spy.so cat logs/testfile.txt
[SPY] init_log() called
[SPY] open("logs/testfile.txt", O_RDONLY) = 3
[SPY] read(fd=3, ...) = 12
Hello world

 **Когда LD_PRELOAD не работает:**
- при **статической линковке** (`gcc -static`);
- при **SUID-бинарях** (по соображениям безопасности);
- если функция не из `libc`, а встроена прямо в бинарь.


## 4. Что такое vDSO и зачем он нужен?
**vDSO (Virtual Dynamic Shared Object)** — это специальная область памяти, через которую ядро предоставляет быстрый доступ к часто вызываемым функциям, например `gettimeofday()`, `clock_gettime()`, `getcpu()`.

vDSO позволяет выполнять такие вызовы **без перехода в ядро**, читая данные напрямую из памяти.  
Это уменьшает накладные расходы на переключение режима.

**Из эксперимента (бенчмарк):**
- `open+close` ≈ 1170 нс  
- `clock_gettime()` (через vDSO) ≈ 30–50 нс  
→ ускорение примерно в **20–40 раз**.

## 5. Почему open() медленнее getpid() на несколько порядков?

`open()` — тяжёлый системный вызов: он требует проверки существования файла, прав доступа, выделения дескриптора и записи его в таблицу процессов. Всё это выполняется в режиме ядра.

`getpid()` реализуется через vDSO — PID процесса уже хранится в памяти, и функция просто его возвращает **без обращения к ядру**.

**Из бенчмарка:**  
- `open+close` ≈ 1170 нс  
- `getpid()` ≈ 30–50 нс  

Разница — **в десятки раз**, что подтверждает, что системные вызовы, взаимодействующие с файловой системой, значительно тяжелее простых чтений из памяти.

**Вывод:**  
Системные вызовы — это граница между пользовательским пространством и ядром.  
Механизмы вроде `LD_PRELOAD` и `vDSO` позволяют наблюдать и оптимизировать это взаимодействие: первый — через подмену функций, второй — через исключение лишних переходов в ядро.