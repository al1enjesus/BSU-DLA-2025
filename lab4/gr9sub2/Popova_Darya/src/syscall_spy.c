#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

static void print_open_flags(int flags, FILE *out) {
    int first = 1;
    if ((flags & O_RDONLY) == O_RDONLY) { fprintf(out, "O_RDONLY"); first = 0; }
    if (flags & O_WRONLY) { if (!first) fprintf(out, "|"); fprintf(out, "O_WRONLY"); first = 0; }
    if (flags & O_RDWR)   { if (!first) fprintf(out, "|"); fprintf(out, "O_RDWR"); first = 0; }
    if (flags & O_CREAT)  { if (!first) fprintf(out, "|"); fprintf(out, "O_CREAT"); first = 0; }
    if (flags & O_TRUNC)  { if (!first) fprintf(out, "|"); fprintf(out, "O_TRUNC"); first = 0; }
    if (flags & O_APPEND) { if (!first) fprintf(out, "|"); fprintf(out, "O_APPEND"); first = 0; }
    if (first) fprintf(out, "0");
}

int open(const char *pathname, int flags, ...) {
    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) {
        original_open = dlsym(RTLD_NEXT, "open");
        if (!original_open) {
            fprintf(stderr, "[SPY] dlsym(open) failed: %s
", dlerror());
        }
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int res = original_open ? original_open(pathname, flags, mode) : -1;

    fprintf(stderr, "[SPY] open(\"%s\", flags=", pathname ? pathname : "(null)");
    print_open_flags(flags, stderr);
    if (flags & O_CREAT) fprintf(stderr, ", mode=0%o", mode);
    fprintf(stderr, ") = %d
", res);

    return res;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    static int (*original_openat)(int, const char*, int, ...) = NULL;
    if (!original_openat) {
        original_openat = dlsym(RTLD_NEXT, "openat");
        if (!original_openat) {
            fprintf(stderr, "[SPY] dlsym(openat) failed: %s
", dlerror());
        }
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int res = original_openat ? original_openat(dirfd, pathname, flags, mode) : -1;

    const char *df = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
    fprintf(stderr, "[SPY] openat(%s, \"%s\", flags=", df, pathname ? pathname : "(null)");
    print_open_flags(flags, stderr);
    if (flags & O_CREAT) fprintf(stderr, ", mode=0%o", mode);
    fprintf(stderr, ") = %d
", res);

    return res;
}

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
        if (!original_read) {
            fprintf(stderr, "[SPY] dlsym(read) failed: %s
", dlerror());
        }
    }

    ssize_t res = original_read ? original_read(fd, buf, count) : -1;

    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd
", fd, buf, count, res);

    return res;
}

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
        if (!original_write) {
        }
    }

    ssize_t res = original_write ? original_write(fd, buf, count) : -1;

    if (fd != 2) {
        if ((fd == 1 || fd == STDOUT_FILENO) && count > 0 && count < 200) {
            fprintf(stderr, "[SPY] write(fd=%d, \"%.*s\", count=%zu) = %zd
",
                    fd, (int)count, (const char*)buf, count, res);
        } else {
            fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd
",
                    fd, buf, count, res);
        }
    }

    return res;
}

int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
        if (!original_close) {
            fprintf(stderr, "[SPY] dlsym(close) failed: %s
", dlerror());
        }
    }

    int res = original_close ? original_close(fd) : -1;

    fprintf(stderr, "[SPY] close(fd=%d) = %d
", fd, res);

    return res;
}