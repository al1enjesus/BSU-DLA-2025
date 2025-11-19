#define _GNU_SOURCE
#include <dlfcn.h>      // dlsym
#include <stdio.h>
#include <fcntl.h>      // O_RDONLY, AT_FDCWD и т.д.
#include <stdarg.h>     // va_list для variadic функций
#include <unistd.h>

// ═════════════════════════════════════════════════════════════
// Перехват open()
// ═════════════════════════════════════════════════════════════
int open(const char *pathname, int flags, ...) {
    // Получаем указатель на оригинальную функцию
    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) {
        original_open = dlsym(RTLD_NEXT, "open");
    }

    // Обработка необязательного аргумента mode (для O_CREAT)
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    // Вызываем оригинальную функцию
    int result = original_open(pathname, flags, mode);

    // Логируем вызов
    fprintf(stderr, "[SPY] open(\"%s\", flags=0x%x) = %d\n",
            pathname, flags, result);

    return result;
}

// ═════════════════════════════════════════════════════════════
// Перехват openat()
// ═════════════════════════════════════════════════════════════
int openat(int dirfd, const char *pathname, int flags, ...) {
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

    // Красивое форматирование dirfd
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
    fprintf(stderr, "[SPY] openat(%s, \"%s\", 0x%x) = %d\n",
            dirfd_str, pathname, flags, result);

    return result;
}

// ═════════════════════════════════════════════════════════════
// Перехват read()
// ═════════════════════════════════════════════════════════════
ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
    }

    ssize_t result = original_read(fd, buf, count);

    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd\n",
            fd, buf, count, result);

    return result;
}

// ═════════════════════════════════════════════════════════════
// Перехват write()
// ═════════════════════════════════════════════════════════════
ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
    }

    ssize_t result = original_write(fd, buf, count);

    // НЕ логируем write() на stderr (иначе бесконечная рекурсия!)
    if (fd != 2) {  // 2 = stderr
        fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd\n",
                fd, buf, count, result);
    }

    return result;
}

// ═════════════════════════════════════════════════════════════
// Перехват close()
// ═════════════════════════════════════════════════════════════
int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
    }

    int result = original_close(fd);

    fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, result);

    return result;
}