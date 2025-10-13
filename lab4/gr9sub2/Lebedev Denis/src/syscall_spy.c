#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static FILE *spy_log_stream = NULL;
static int log_fd = -1;

// Вспомогательная функция для безопасного логирования
static void spy_log(const char *fmt, ...) {
    if (!spy_log_stream) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(spy_log_stream, fmt, args);
    va_end(args);
    fflush(spy_log_stream);
}

__attribute__((constructor))
void spy_init(void) {
    log_fd = dup(STDERR_FILENO);
    if (log_fd == -1) {
        perror("spy_init: dup");
        spy_log_stream = stderr;
        log_fd = STDERR_FILENO;
        return;
    }

    spy_log_stream = fdopen(log_fd, "w");
    if (!spy_log_stream) {
        perror("spy_init: fdopen");
        spy_log_stream = stderr;
        log_fd = STDERR_FILENO;
    }
}

__attribute__((destructor))
void spy_cleanup(void) {
    if (spy_log_stream && log_fd != STDERR_FILENO) {
        if (fclose(spy_log_stream) == EOF) {
            perror("spy_cleanup: fclose");
        }
    }
}

// --- Обертки системных вызовов ---

int open(const char *pathname, int flags, ...) {
    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) {
        original_open = dlsym(RTLD_NEXT, "open");
        if (!original_open) {
            fprintf(stderr, "spy: failed to find original open: %s\n", dlerror());
            return -1;
        }
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = original_open(pathname, flags, mode);
    int err = errno;
    spy_log("[SPY] open(\"%s\", flags=0x%x) = %d%s\n",
            pathname, flags, result, result == -1 ? strerror(err) : "");
    errno = err;
    return result;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    static int (*original_openat)(int, const char*, int, ...) = NULL;
    if (!original_openat) {
        original_openat = dlsym(RTLD_NEXT, "openat");
        if (!original_openat) {
            fprintf(stderr, "spy: failed to find original openat: %s\n", dlerror());
            return -1;
        }
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = original_openat(dirfd, pathname, flags, mode);
    int err = errno;
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
    spy_log("[SPY] openat(%s, \"%s\", 0x%x) = %d%s\n",
            dirfd_str, pathname, flags, result, result == -1 ? strerror(err) : "");
    errno = err;
    return result;
}

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
        if (!original_read) {
            fprintf(stderr, "spy: failed to find original read: %s\n", dlerror());
            return -1;
        }
    }
    ssize_t result = original_read(fd, buf, count);
    int err = errno;
    spy_log("[SPY] read(fd=%d, buf=%p, count=%zu) = %zd%s\n",
            fd, buf, count, result, result == -1 ? strerror(err) : "");
    errno = err;
    return result;
}

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
        if (!original_write) {
            fprintf(stderr, "spy: failed to find original write: %s\n", dlerror());
            return -1;
        }
    }

    ssize_t result = original_write(fd, buf, count);
    int err = errno;

    // Не логируем записи в наш собственный лог
    if (fd != log_fd) {
        spy_log("[SPY] write(fd=%d, buf=%p, count=%zu) = %zd%s\n",
                fd, buf, count, result, result == -1 ? strerror(err) : "");
    }
    errno = err;
    return result;
}

int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
        if (!original_close) {
            fprintf(stderr, "spy: failed to find original close: %s\n", dlerror());
            return -1;
        }
    }
    int result = original_close(fd);
    int err = errno;
    spy_log("[SPY] close(fd=%d) = %d%s\n", fd, result, result == -1 ? strerror(err) : "");
    errno = err;
    return result;
}

int dup(int oldfd) {
    static int (*original_dup)(int) = NULL;
    if (!original_dup) {
        original_dup = dlsym(RTLD_NEXT, "dup");
        if (!original_dup) {
            int err = errno;
            spy_log("spy: failed to find original dup: %s\n", dlerror());
            errno = err;
            return -1;
        }
    }

    int saved_errno = errno;
    spy_log("[SPY] dup(%d)\n", oldfd);
    fprintf(stderr, "[SPY] dup(%d)\n", oldfd);  // дублирование в консоль
    fflush(stderr);
    errno = saved_errno;

    return original_dup(oldfd);
}
