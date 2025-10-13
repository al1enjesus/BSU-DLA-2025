#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

static FILE *spy_log_stream = NULL;
static int log_fd = -1;

__attribute__((constructor))
void spy_init(void) {
    log_fd = dup(STDERR_FILENO);
    if (log_fd != -1) {
        spy_log_stream = fdopen(log_fd, "w");
    }

    if (spy_log_stream == NULL) {
        spy_log_stream = stderr;
        log_fd = STDERR_FILENO;
    }
}

__attribute__((destructor))
void spy_cleanup(void) {
    if (spy_log_stream && log_fd != STDERR_FILENO) {
        fclose(spy_log_stream);
    }
}

int open(const char *pathname, int flags, ...) {
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
    int err = errno;
    fprintf(spy_log_stream, "[SPY] open(\"%s\", flags=0x%x) = %d\n", pathname, flags, result);
    fflush(spy_log_stream);
    errno = err;
    return result;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
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
    int err = errno;
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
    fprintf(spy_log_stream, "[SPY] openat(%s, \"%s\", 0x%x) = %d\n", dirfd_str, pathname, flags, result);
    fflush(spy_log_stream);
    errno = err;
    return result;
}

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) original_read = dlsym(RTLD_NEXT, "read");
    ssize_t result = original_read(fd, buf, count);
    int err = errno;
    fprintf(spy_log_stream, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, result);
    fflush(spy_log_stream);
    errno = err;
    return result;
}

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) original_write = dlsym(RTLD_NEXT, "write");
    ssize_t result = original_write(fd, buf, count);
    int err = errno;

    if (fd != log_fd) {
        fprintf(spy_log_stream, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, result);
        fflush(spy_log_stream);
    }
    errno = err;
    return result;
}

int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) original_close = dlsym(RTLD_NEXT, "close");
    int result = original_close(fd);
    int err = errno;
    fprintf(spy_log_stream, "[SPY] close(fd=%d) = %d\n", fd, result);
    fflush(spy_log_stream);
    errno = err;
    return result;
}

int dup(int oldfd) {
    static int (*original_dup)(int) = NULL;
    if (!original_dup) {
        original_dup = dlsym(RTLD_NEXT, "dup");
    }

    fprintf(stderr, "[SPY] dup(%d)\n", oldfd);
    fflush(stderr);
    return original_dup(oldfd);
}