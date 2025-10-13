#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

// Указатели на оригинальные функции
static int (*original_open)(const char*, int, ...) = NULL;
static int (*original_openat)(int, const char*, int, ...) = NULL;
static ssize_t (*original_read)(int, void*, size_t) = NULL;
static ssize_t (*original_write)(int, const void*, size_t) = NULL;
static int (*original_close)(int) = NULL;

// Макрос для инициализации указателя на оригинальную функцию
#define INIT_ORIGINAL_FUNC(name) \
    if (!original_##name) { \
        original_##name = dlsym(RTLD_NEXT, #name); \
    }

// Перехват open()
int open(const char *pathname, int flags, ...) {
    INIT_ORIGINAL_FUNC(open);

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

// Перехват openat()
int openat(int dirfd, const char *pathname, int flags, ...) {
    INIT_ORIGINAL_FUNC(openat);

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

// Перехват read()
ssize_t read(int fd, void *buf, size_t count) {
    INIT_ORIGINAL_FUNC(read);
    ssize_t result = original_read(fd, buf, count);
    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, result);
    return result;
}

// Перехват write()
ssize_t write(int fd, const void *buf, size_t count) {
    INIT_ORIGINAL_FUNC(write);
    ssize_t result = original_write(fd, buf, count);
    if (fd != 2) { // Избегаем рекурсии, не логируя запись в stderr
        fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, result);
    }
    return result;
}

// Перехват close()
int close(int fd) {
    INIT_ORIGINAL_FUNC(close);
    int result = original_close(fd);
    fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, result);
    return result;
}
