Лабораторная 4 — Системные вызовы
Выбор программ
Номер в списке группы: 23
Расчёт группы программ: 23 % 4 = 3
Программы для анализа: gcc, make, as
Мои задания: A (LD_PRELOAD), B (Benchmark) — обязательные

Среда выполнения
ОС: WSL2 Ubuntu 22.04.3 LTS

Ядро: 5.15.146.1-microsoft-standard-WSL2

Процессор: AMD Ryzen 7 2700 CPU @ 3.20GHz

Компилятор: gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0

Задание A: LD_PRELOAD перехват
Код библиотеки
Библиотека wsl_spy.so перехватывает следующие системные вызовы:

open(), openat(), open64(), openat64() - открытие файлов

read(), write() - чтение/запись данных

close() - закрытие файловых дескрипторов

stat(), lstat(), fstat() и их 64-битные версии - получение информации о файлах

access(), faccessat() - проверка прав доступа

opendir(), readdir(), closedir() - работа с директориями

Важное примечание: При тестировании простых команд типа gcc --version, make --version, as --version системные вызовы не успевают отобразиться, так как программы выполняются слишком быстро. Поэтому для анализа использовались реальные операции компиляции.
Исходный код wsl_spy.c:
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>

// Функция инициализации - вызывается при загрузке библиотеки
__attribute__((constructor)) void init_library() {
    fprintf(stderr, "[SPY] Library loaded successfully! (WSL version)\n");
}

// Упрощенный вывод флагов
const char* get_flags_str(int flags) {
    if (flags == O_RDONLY) return "O_RDONLY";
    if (flags == O_WRONLY) return "O_WRONLY"; 
    if (flags == O_RDWR) return "O_RDWR";
    return "UNKNOWN";
}

// ========== ПЕРЕХВАТ ФАЙЛОВЫХ ОПЕРАЦИЙ ==========

// open - устаревший, но для совместимости
int open(const char *pathname, int flags, ...) {
    static int (*orig_open)(const char*, int, ...) = NULL;
    if (!orig_open) orig_open = dlsym(RTLD_NEXT, "open");
    
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    int result = orig_open(pathname, flags, mode);
    fprintf(stderr, "[SPY] open('%s', %s) = %d\n", pathname, get_flags_str(flags & O_ACCMODE), result);
    return result;
}

// openat - основной системный вызов в современных системах
int openat(int dirfd, const char *pathname, int flags, ...) {
    static int (*orig_openat)(int, const char*, int, ...) = NULL;
    if (!orig_openat) orig_openat = dlsym(RTLD_NEXT, "openat");
    
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    int result = orig_openat(dirfd, pathname, flags, mode);
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "dirfd";
    fprintf(stderr, "[SPY] openat(%s, '%s', %s) = %d\n", dirfd_str, pathname, get_flags_str(flags & O_ACCMODE), result);
    return result;
}

// open64 - 64-битная версия
int open64(const char *pathname, int flags, ...) {
    static int (*orig_open64)(const char*, int, ...) = NULL;
    if (!orig_open64) orig_open64 = dlsym(RTLD_NEXT, "open64");
    
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    int result = orig_open64(pathname, flags, mode);
    fprintf(stderr, "[SPY] open64('%s', %s) = %d\n", pathname, get_flags_str(flags & O_ACCMODE), result);
    return result;
}

// openat64 - 64-битная версия openat
int openat64(int dirfd, const char *pathname, int flags, ...) {
    static int (*orig_openat64)(int, const char*, int, ...) = NULL;
    if (!orig_openat64) orig_openat64 = dlsym(RTLD_NEXT, "openat64");
    
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    int result = orig_openat64(dirfd, pathname, flags, mode);
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "dirfd";
    fprintf(stderr, "[SPY] openat64(%s, '%s', %s) = %d\n", dirfd_str, pathname, get_flags_str(flags & O_ACCMODE), result);
    return result;
}

// ========== ПЕРЕХВАТ ЧТЕНИЯ/ЗАПИСИ ==========

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*orig_read)(int, void*, size_t) = NULL;
    if (!orig_read) orig_read = dlsym(RTLD_NEXT, "read");
    
    ssize_t result = orig_read(fd, buf, count);
    fprintf(stderr, "[SPY] read(fd=%d, count=%zu) = %zd\n", fd, count, result);
    return result;
}

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*orig_write)(int, const void*, size_t) = NULL;
    if (!orig_write) orig_write = dlsym(RTLD_NEXT, "write");
    
    // Избегаем рекурсии для stderr
    ssize_t result = orig_write(fd, buf, count);
    if (fd != 2) { // не stderr
        fprintf(stderr, "[SPY] write(fd=%d, count=%zu) = %zd\n", fd, count, result);
    }
    return result;
}

// ========== ПЕРЕХВАТ ЗАКРЫТИЯ ==========

int close(int fd) {
    static int (*orig_close)(int) = NULL;
    if (!orig_close) orig_close = dlsym(RTLD_NEXT, "close");
    
    int result = orig_close(fd);
    fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, result);
    return result;
}

// ========== ПЕРЕХВАТ ФУНКЦИЙ СТАТ ==========

int stat(const char *pathname, struct stat *statbuf) {
    static int (*orig_stat)(const char*, struct stat*) = NULL;
    if (!orig_stat) orig_stat = dlsym(RTLD_NEXT, "stat");
    
    int result = orig_stat(pathname, statbuf);
    fprintf(stderr, "[SPY] stat('%s') = %d\n", pathname, result);
    return result;
}

int lstat(const char *pathname, struct stat *statbuf) {
    static int (*orig_lstat)(const char*, struct stat*) = NULL;
    if (!orig_lstat) orig_lstat = dlsym(RTLD_NEXT, "lstat");
    
    int result = orig_lstat(pathname, statbuf);
    fprintf(stderr, "[SPY] lstat('%s') = %d\n", pathname, result);
    return result;
}

int fstat(int fd, struct stat *statbuf) {
    static int (*orig_fstat)(int, struct stat*) = NULL;
    if (!orig_fstat) orig_fstat = dlsym(RTLD_NEXT, "fstat");
    
    int result = orig_fstat(fd, statbuf);
    fprintf(stderr, "[SPY] fstat(fd=%d) = %d\n", fd, result);
    return result;
}

// 64-битные версии
int stat64(const char *pathname, struct stat64 *statbuf) {
    static int (*orig_stat64)(const char*, struct stat64*) = NULL;
    if (!orig_stat64) orig_stat64 = dlsym(RTLD_NEXT, "stat64");
    
    int result = orig_stat64(pathname, statbuf);
    fprintf(stderr, "[SPY] stat64('%s') = %d\n", pathname, result);
    return result;
}

int lstat64(const char *pathname, struct stat64 *statbuf) {
    static int (*orig_lstat64)(const char*, struct stat64*) = NULL;
    if (!orig_lstat64) orig_lstat64 = dlsym(RTLD_NEXT, "lstat64");
    
    int result = orig_lstat64(pathname, statbuf);
    fprintf(stderr, "[SPY] lstat64('%s') = %d\n", pathname, result);
    return result;
}

int fstat64(int fd, struct stat64 *statbuf) {
    static int (*orig_fstat64)(int, struct stat64*) = NULL;
    if (!orig_fstat64) orig_fstat64 = dlsym(RTLD_NEXT, "fstat64");
    
    int result = orig_fstat64(fd, statbuf);
    fprintf(stderr, "[SPY] fstat64(fd=%d) = %d\n", fd, result);
    return result;
}

// ========== ПЕРЕХВАТ ДИРЕКТОРИЙ ==========

DIR *opendir(const char *name) {
    static DIR *(*orig_opendir)(const char*) = NULL;
    if (!orig_opendir) orig_opendir = dlsym(RTLD_NEXT, "opendir");
    
    DIR *result = orig_opendir(name);
    fprintf(stderr, "[SPY] opendir('%s') = %p\n", name, result);
    return result;
}

struct dirent *readdir(DIR *dirp) {
    static struct dirent *(*orig_readdir)(DIR*) = NULL;
    if (!orig_readdir) orig_readdir = dlsym(RTLD_NEXT, "readdir");
    
    struct dirent *result = orig_readdir(dirp);
    if (result) {
        fprintf(stderr, "[SPY] readdir(%p) = '%s'\n", dirp, result->d_name);
    }
    return result;
}

int closedir(DIR *dirp) {
    static int (*orig_closedir)(DIR*) = NULL;
    if (!orig_closedir) orig_closedir = dlsym(RTLD_NEXT, "closedir");
    
    int result = orig_closedir(dirp);
    fprintf(stderr, "[SPY] closedir(%p) = %d\n", dirp, result);
    return result;
}

// ========== ПЕРЕХВАТ ACCESS ==========

int access(const char *pathname, int mode) {
    static int (*orig_access)(const char*, int) = NULL;
    if (!orig_access) orig_access = dlsym(RTLD_NEXT, "access");
    
    int result = orig_access(pathname, mode);
    fprintf(stderr, "[SPY] access('%s', 0x%x) = %d\n", pathname, mode, result);
    return result;
}

int faccessat(int dirfd, const char *pathname, int mode, int flags) {
    static int (*orig_faccessat)(int, const char*, int, int) = NULL;
    if (!orig_faccessat) orig_faccessat = dlsym(RTLD_NEXT, "faccessat");
    
    int result = orig_faccessat(dirfd, pathname, mode, flags);
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "dirfd";
    fprintf(stderr, "[SPY] faccessat(%s, '%s', 0x%x, 0x%x) = %d\n", 
            dirfd_str, pathname, mode, flags, result);
    return result;
}
Эксперимент 1: Программа gcc
Команда:

bash
LD_PRELOAD=./wsl_spy.so gcc hello.c -o hello
Полный вывод (первые 40 строк):

text
[SPY] Library loaded successfully! (WSL version)
[SPY] access('/usr/local/sbin/gcc', 0x1) = -1
[SPY] access('/usr/local/bin/gcc', 0x1) = -1
[SPY] access('/usr/sbin/gcc', 0x1) = -1
[SPY] access('/usr/bin/gcc', 0x1) = 0
[SPY] stat('/usr/bin/gcc') = 0
[SPY] access('/usr/local/sbin/gcc', 0x1) = -1
[SPY] access('/usr/local/bin/gcc', 0x1) = -1
[SPY] access('/usr/sbin/gcc', 0x1) = -1
[SPY] access('/usr/bin/gcc', 0x1) = 0
[SPY] stat('/usr/bin/gcc') = 0
[SPY] access('/usr/lib/gcc/x86_64-linux-gnu/11/', 0x1) = 0
[SPY] access('/usr/lib/gcc/x86_64-linux-gnu/11/', 0x1) = 0
[SPY] access('/usr/lib/gcc/x86_64-linux-gnu/11/specs', 0x4) = -1
[SPY] access('/usr/lib/gcc/x86_64-linux-gnu/11/../../../../x86_64-linux-gnu/lib/x86_64-linux-gnu/11/specs', 0x4) = -1
[SPY] access('/usr/lib/gcc/x86_64-linux-gnu/11/../../../../x86_64-linux-gnu/lib/specs', 0x4) = -1
[SPY] access('/usr/lib/gcc/x86_64-linux-gnu/specs', 0x4) = -1
[SPY] access('/usr/lib/gcc/x86_64-linux-gnu/11/', 0x1) = 0
[SPY] stat('/usr/lib/gcc/x86_64-linux-gnu/11/lto-wrapper') = 0
[SPY] access('/usr/lib/gcc/x86_64-linux-gnu/11/lto-wrapper', 0x1) = 0
[SPY] stat('/usr/lib/gcc/x86_64-linux-gnu/11/cc1') = 0
[SPY] access('/usr/lib/gcc/x86_64-linux-gnu/11/cc1', 0x1) = 0
[SPY] open('hello.c', O_RDONLY) = 3
[SPY] fstat(fd=3) = 0
[SPY] read(fd=3, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] open('/usr/include/x86_64-linux-gnu/bits/wordsize.h', O_RDONLY) = 3
[SPY] fstat(fd=3) = 0
[SPY] read(fd=3, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] open('/usr/include/x86_64-linux-gnu/bits/timesize.h', O_RDONLY) = 3
[SPY] fstat(fd=3) = 0
[SPY] read(fd=3, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] open('/usr/include/features.h', O_RDONLY) = 3
[SPY] fstat(fd=3) = 0
[SPY] read(fd=3, count=832) = 832
[SPY] close(fd=3) = 0
[SPY] open('/usr/include/features-time64.h', O_RDONLY) = 3
Анализ:

Всего вызовов: 671

Какие файлы открывались: системные заголовочные файлы (/usr/include/x86_64-linux-gnu/bits/wordsize.h, /usr/include/features.h), файлы компилятора (/usr/lib/gcc/x86_64-linux-gnu/11/cc1), исходный файл (hello.c)

Неожиданности: множественные проверки доступа к разным путям gcc, поиск спецификаций компилятора

Эксперимент 2: Программа make
Команда:

bash
LD_PRELOAD=./wsl_spy.so make hello
Полный вывод (первые 30 строк):

text
[SPY] Library loaded successfully! (WSL version)
[SPY] stat('/usr/gnu/include') = -1
[SPY] stat('/usr/local/include') = 0
[SPY] stat('/usr/include') = 0
[SPY] stat('/usr/include') = 0
[SPY] stat('.') = 0
[SPY] opendir('.') = 0x643b74213440
[SPY] readdir(0x643b74213440) = 'make_full.log'
[SPY] readdir(0x643b74213440) = 'script.sh'
[SPY] readdir(0x643b74213440) = 'make_full.log'
[SPY] readdir(0x643b74213440) = 'syscall_spy.c'
[SPY] readdir(0x643b74213440) = 'gcc_full.log'
[SPY] readdir(0x643b74213440) = 'as_full.log'
[SPY] readdir(0x643b74213440) = 'hello.c'
[SPY] readdir(0x643b74213440) = '..'
[SPY] readdir(0x643b74213440) = 'hello'
[SPY] readdir(0x643b74213440) = 'wsl_spy.c'
[SPY] readdir(0x643b74213440) = '.'
[SPY] readdir(0x643b74213440) = 'wsl_spy.so'
[SPY] closedir(0x643b74213440) = 0
[SPY] stat('RCS') = -1
[SPY] stat('SCCS') = -1
[SPY] stat('GNUmakefile') = -1
[SPY] stat('makefile') = -1
[SPY] stat('Makefile') = 0
[SPY] stat('hello') = 0
[SPY] stat('hello.c') = 0
make: 'hello' is up to date.
Анализ:

Всего вызовов: 29

Какие файлы открывались: проверка различных makefile-ов (GNUmakefile, makefile, Makefile), чтение текущей директории

Неожиданности: make проверяет несколько альтернативных имен makefile-ов перед использованием Makefile

Эксперимент 3: Программа as (ассемблер)
Команда:

bash
LD_PRELOAD=./wsl_spy.so as test.s -o test.o
Полный вывод:

text
[SPY] Library loaded successfully! (WSL version)
[SPY] stat('test.o') = -1
[SPY] stat('test.o') = -1
[SPY] lstat('test.o') = 0
Анализ:

Всего вызовов: 4

Какие файлы открывались: проверка существования выходного файла test.o

Неожиданности: as делает очень мало системных вызовов по сравнению с gcc

Сравнительная таблица
Программа	Всего вызовов	open/openat	read	write	close	stat	access	opendir/readdir
gcc	671	40+	31	0	35	40	35+	0
make	29	0	0	0	1	9	0	16
as	4	0	0	0	0	3	0	0
ОБЯЗАТЕЛЬНЫЙ АНАЛИЗ И РАССУЖДЕНИЯ
Особенности сбора логов:

При тестировании простых команд типа gcc --version, make --version, as --version системные вызовы не успевают отобразиться, так как программы выполняются слишком быстро. Поэтому для анализа использовались реальные операции компиляции:

GCC: компиляция реального C файла gcc hello.c -o hello

MAKE: выполнение реальной сборки make hello

AS: ассемблирование реального asm файла as test.s -o test.o

Это демонстрирует важность использования реалистичных рабочих нагрузок для анализа системных вызовов, а не просто версионных команд.

Какая программа делает больше всего системных вызовов? Почему?

GCC делает значительно больше системных вызовов (671) по сравнению с make (29) и as (4). Это объясняется природой работы компилятора:

GCC должен найти и прочитать множество заголовочных файлов

Проверить доступность различных компонентов toolchain

Обработать зависимости и пути поиска библиотек

Выполнить препроцессинг, компиляцию и линковку

Какие неожиданные файлы открываются?

Системные библиотеки: /lib/x86_64-linux-gnu/libc.so.6 - используется как основная C библиотека

Конфигурационные файлы: /etc/ld.so.cache не виден, но GCC ищет файлы спецификаций компилятора

Псевдо-файлы: не обнаружены в этом эксперименте, но в других случаях могут читаться /proc/cpuinfo, /proc/version

Сравните профили вызовов:

GCC: много stat() и access() для поиска файлов, много open()/read()/close() для чтения заголовков

MAKE: в основном readdir() для сканирования директории и stat() для проверки временных меток

AS: только несколько stat() вызовов для проверки файлов

Что узнали нового о работе программ?

Компиляторы делают огромное количество проверок файловой системы

Make использует эвристики для поиска makefile-ов

Ассемблер работает значительно проще чем полноценный компилятор

Инструменты разработки активно используют кэширование и поиск по путям

Важный вывод: простые команды (типа --version) не отражают реальную картину системных вызовов - нужны реальные рабочие нагрузки

Эксперимент со статической программой
Команда:

bash
gcc -static -o hello_static hello.c
LD_PRELOAD=./wsl_spy.so ./hello_static
Результат:


Hello from compilation test!
Объяснение: LD_PRELOAD не работает на статических программах потому что:

Статические программы содержат весь необходимый код внутри исполняемого файла

Не используется динамический линкер для загрузки внешних библиотек

Все символы разрешаются на этапе компиляции, а не во время выполнения

Механизм LD_PRELOAD работает через перехват вызовов динамического линкера

Задание B: Benchmark системных вызовов
Исходный код benchmark.c
c
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <x86intrin.h>
#include <time.h>
#include <stdint.h> 

// Пустая функция для сравнения
__attribute__((noinline)) int dummy() {
    return 42;
}

// Измерение времени в тактах
uint64_t measure_cycles(void (*func)(), int iterations) {
    uint64_t start = __rdtsc();
    func(iterations);
    uint64_t end = __rdtsc();
    return end - start;
}

// Измерение времени в наносекундах
long measure_ns(void (*func)(), int iterations) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    func(iterations);
    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
}

// Тестовые функции
void test_dummy(int iterations) {
    volatile int result;
    for (int i = 0; i < iterations; i++) {
        result = dummy();
    }
    (void)result;
}

void test_getpid(int iterations) {
    volatile pid_t result;
    for (int i = 0; i < iterations; i++) {
        result = getpid();
    }
    (void)result;
}

void test_open_close(int iterations) {
    for (int i = 0; i < iterations; i++) {
        int fd = open("/tmp/benchmark_test", O_RDWR | O_CREAT, 0644);
        if (fd != -1) {
            close(fd);
        }
    }
    remove("/tmp/benchmark_test");
}

void test_gettimeofday_vdso(int iterations) {
    struct timeval tv;
    for (int i = 0; i < iterations; i++) {
        gettimeofday(&tv, NULL);
    }
}

void test_read(int iterations) {
    char buffer[100];
    int fd = open("/dev/zero", O_RDONLY);
    if (fd == -1) {
        perror("open /dev/zero");
        return;
    }
    
    for (int i = 0; i < iterations; i++) {
        read(fd, buffer, 10);
    }
    close(fd);
}

void test_write(int iterations) {
    char buffer[] = "test";
    int fd = open("/dev/null", O_WRONLY);
    if (fd == -1) {
        perror("open /dev/null");
        return;
    }
    
    for (int i = 0; i < iterations; i++) {
        write(fd, buffer, 5);
    }
    close(fd);
}

int main() {
    const int iterations = 1000000;
    const int file_iterations = 100000; // Меньше для файловых операций
    
    printf("=== System Call Benchmark ===\n\n");
    printf("Iterations: %d (file operations: %d)\n\n", iterations, file_iterations);
    
    // Warmup
    test_dummy(1000);
    
    // Измерение dummy()
    long ns_dummy = measure_ns(test_dummy, iterations);
    uint64_t cycles_dummy = measure_cycles(test_dummy, iterations);
    
    // Измерение getpid()
    long ns_getpid = measure_ns(test_getpid, iterations);
    uint64_t cycles_getpid = measure_cycles(test_getpid, iterations);
    
    // Измерение open+close
    long ns_open_close = measure_ns(test_open_close, file_iterations);
    uint64_t cycles_open_close = measure_cycles(test_open_close, file_iterations);
    
    // Масштабируем до 1M итераций
    ns_open_close = (ns_open_close * iterations) / file_iterations;
    cycles_open_close = (cycles_open_close * iterations) / file_iterations;
    
    // Измерение gettimeofday()
    long ns_gettimeofday = measure_ns(test_gettimeofday_vdso, iterations);
    uint64_t cycles_gettimeofday = measure_cycles(test_gettimeofday_vdso, iterations);
    
    // Измерение read
    long ns_read = measure_ns(test_read, iterations);
    uint64_t cycles_read = measure_cycles(test_read, iterations);
    
    // Измерение write
    long ns_write = measure_ns(test_write, iterations);
    uint64_t cycles_write = measure_cycles(test_write, iterations);
    
    // Вывод результатов
    printf("| Operation              | Time (ns) | CPU Cycles | Slowdown vs userspace |\n");
    printf("|------------------------|-----------|------------|-----------------------|\n");
    printf("| dummy() userspace      | %7.1f  | %9.1f | 1.0x                  |\n", 
           (double)ns_dummy / iterations, (double)cycles_dummy / iterations);
    printf("| getpid()               | %7.1f  | %9.1f | %5.1fx                |\n", 
           (double)ns_getpid / iterations, (double)cycles_getpid / iterations, 
           (double)ns_getpid / ns_dummy);
    printf("| open()+close()         | %7.1f  | %9.1f | %5.1fx                |\n", 
           (double)ns_open_close / iterations, (double)cycles_open_close / iterations,
           (double)ns_open_close / ns_dummy);
    printf("| gettimeofday() vDSO    | %7.1f  | %9.1f | %5.1fx                |\n", 
           (double)ns_gettimeofday / iterations, (double)cycles_gettimeofday / iterations,
           (double)ns_gettimeofday / ns_dummy);
    printf("| read()                 | %7.1f  | %9.1f | %5.1fx                |\n",
           (double)ns_read / iterations, (double)cycles_read / iterations,
           (double)ns_read / ns_dummy);
    printf("| write()                | %7.1f  | %9.1f | %5.1fx                |\n",
           (double)ns_write / iterations, (double)cycles_write / iterations,
           (double)ns_write / ns_dummy);
    
    printf("\n=== Additional Info ===\n");
    printf("CPU Frequency: ~%.1f GHz\n", (double)cycles_dummy / ns_dummy);
    printf("Test file: /tmp/benchmark_test\n");
    printf("Special devices: /dev/zero, /dev/null\n");
    
    return 0;
}
Таблица результатов
Операция	Время (ns)	Циклов CPU	Во сколько раз медленнее userspace
dummy() userspace	0.3	0.9	1x (базовая линия)
getpid()	204.2	653.0	699.1x
open()+close()	4678.9	14861.1	16018.0x
gettimeofday() vDSO	27.8	87.9	95.3x
read()	419.3	1329.2	1435.6x
write()	333.3	1068.0	1141.1x
Эксперименты
Perf stat: Недоступен в WSL2 - команда perf не установлена в данной среде

Эксперимент с кэшем: Не выполнялся из-за ограничений WSL2 (требует прав root для сброса кэша)

ОБЯЗАТЕЛЬНЫЙ АНАЛИЗ И РАССУЖДЕНИЯ
Почему системный вызов в 50-100 раз медленнее userspace функции?

Системный вызов требует переключения из user mode в kernel mode (context switch), что включает:

Сохранение регистров CPU

Проверку прав доступа и аргументов

Переключение стека и таблиц страниц

Выполнение кода ядра

Восстановление состояния

Почему open() в ~16000 раз медленнее getpid()?

getpid(): просто читает поле из структуры процесса в памяти ядра (кэшируется)

open(): выполняет полный путь файловой системы - парсинг пути, проверка прав, поиск inode, блокировки, возможное обращение к диску

Что такое vDSO и почему это важно?

vDSO (Virtual Dynamic Shared Object) позволяет выполнять некоторые syscalls без перехода в kernel mode:

gettimeofday() выполняется в userspace через mapping специальной страницы ядра

Избегает context switch, поэтому всего в 95 раз медленнее userspace функции

Другие vDSO функции: clock_gettime(), time(), getcpu()

Практические выводы:

Избегайте частых системных вызовов в performance-critical коде

Используйте буферизацию для операций ввода-вывода

Предпочитайте vDSO-функции там, где это возможно

Учитывайте влияние кэширования на производительность

Ответы на обязательные вопросы
1. Что такое системный вызов и чем он отличается от обычной функции?
Системный вызов - это интерфейс между пользовательскими программами и ядром ОС. В отличие от обычной функции, syscall требует переключения в kernel mode, проверки прав доступа и выполняется в привилегированном режиме.

2. Почему системный вызов медленнее обычной функции?
Из-за overhead переключения контекста, проверок безопасности и необходимости сохранения/восстановления состояния процессора.

3. Как работает LD_PRELOAD и в каких случаях он НЕ работает?
LD_PRELOAD загружает библиотеки перед системными, перехватывая вызовы функций. Не работает со статически слинкованными программами и setuid-бинарниками.

4. Что такое vDSO и зачем он нужен?
vDSO позволяет выполнять часто используемые syscalls без перехода в kernel mode, значительно ускоряя их выполнение.

5. Почему open() медленнее getpid() на несколько порядков?
open() работает с файловой системой (диском), а getpid() просто читает данные из памяти ядра.

Общие выводы
Понимание системных вызовов критически важно для оптимизации производительности

Разные типы программ имеют совершенно разные профили системных вызовов

Инструменты вроде LD_PRELOAD и strace незаменимы для отладки и анализа

Реальные рабочие нагрузки показывают значительно больше системных вызовов чем простые команды

Знание механизмов вроде vDSO помогает писать более эффективный код