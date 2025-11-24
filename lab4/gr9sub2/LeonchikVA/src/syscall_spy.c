#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

// Счётчики
static unsigned long count_open = 0;
static unsigned long count_openat = 0;
static unsigned long count_read = 0;
static unsigned long count_write = 0;
static unsigned long count_close = 0;

// Конструктор - вызывается при загрузке
__attribute__((constructor))
static void init_spy(void) {
    fprintf(stderr, "\n[SPY] === Библиотека перехвата загружена ===\n");
    fprintf(stderr, "[SPY] Студент №23, Программы: gcc, make, as\n\n");
}

// Деструктор - статистика
__attribute__((destructor))
static void print_statistics(void) {
    fprintf(stderr, "\n=== [SPY STATISTICS] ===\n");
    fprintf(stderr, "open():      %lu\n", count_open);
    fprintf(stderr, "openat():    %lu\n", count_openat);
    fprintf(stderr, "read():      %lu\n", count_read);
    fprintf(stderr, "write():     %lu\n", count_write);
    fprintf(stderr, "close():     %lu\n", count_close);
    fprintf(stderr, "ИТОГО:       %lu\n", count_open + count_openat + count_read + count_write + count_close);
    fprintf(stderr, "========================\n\n");
}

// ═════════════════════════════════════════════════════════════
// Перехват open()
// ═════════════════════════════════════════════════════════════
int open(const char *pathname, int flags, ...) {
    count_open++;
    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) original_open = dlsym(RTLD_NEXT, "open");
    
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    int result = original_open(pathname, flags, mode);
    fprintf(stderr, "[SPY] open(\"%s\", flags=0x%x) = %d\n", pathname, flags, result);
    return result;
}

// ═════════════════════════════════════════════════════════════
// Перехват openat()
// ═════════════════════════════════════════════════════════════
int openat(int dirfd, const char *pathname, int flags, ...) {
    count_openat++;
    static int (*original_openat)(int, const char*, int, ...) = NULL;
    if (!original_openat) original_openat = dlsym(RTLD_NEXT, "openat");
    
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    int result = original_openat(dirfd, pathname, flags, mode);
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
    fprintf(stderr, "[SPY] openat(%s, \"%s\", 0x%x) = %d\n", dirfd_str, pathname, flags, result);
    return result;
}

// ═════════════════════════════════════════════════════════════
// Перехват read()
// ИСПРАВЛЕНИЕ: Логируем все вызовы (fd >= 0). 
// Вывод в stderr не вызывает write() -> нет рекурсии.
// ═════════════════════════════════════════════════════════════
ssize_t read(int fd, void *buf, size_t count) {
    count_read++;
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) original_read = dlsym(RTLD_NEXT, "read");
    
    ssize_t result = original_read(fd, buf, count);
    
    // Логируем все чтения, включая системные (fd=0, 1, 2) и файловые (fd > 2)
    fprintf(stderr, "[SPY] read(fd=%d, count=%zu) = %zd\n", fd, count, result);
    
    return result;
}

// ═════════════════════════════════════════════════════════════
// Перехват write()
// ВАЖНО: Сохраняем защиту от рекурсии (не логируем запись в fd=2/stderr)
// ═════════════════════════════════════════════════════════════
ssize_t write(int fd, const void *buf, size_t count) {
    count_write++;
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) original_write = dlsym(RTLD_NEXT, "write");
    
    ssize_t result = original_write(fd, buf, count);
    
    // Проверка fd != 2 (stderr) или fd > 2 (консервативная защита)
    if (fd != 2) { 
        fprintf(stderr, "[SPY] write(fd=%d, count=%zu) = %zd\n", fd, count, result);
    }
    return result;
}

// ═════════════════════════════════════════════════════════════
// Перехват close()
// ИСПРАВЛЕНИЕ: Логируем все вызовы (fd >= 0).
// ═════════════════════════════════════════════════════════════
int close(int fd) {
    count_close++;
    static int (*original_close)(int) = NULL;
    if (!original_close) original_close = dlsym(RTLD_NEXT, "close");
    
    int result = original_close(fd);
    
    // Логируем все закрытия
    fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, result);
    
    return result;
}