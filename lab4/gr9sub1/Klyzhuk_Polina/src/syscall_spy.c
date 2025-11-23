#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

// Переменные-функции для оригиналов
static int (*orig_open)(const char *pathname, int flags, ...) = NULL;
static int (*orig_openat)(int dirfd, const char *pathname, int flags, ...) = NULL;
static ssize_t (*orig_read)(int fd, void *buf, size_t count) = NULL;
static ssize_t (*orig_write)(int fd, const void *buf, size_t count) = NULL;
static int (*orig_close)(int fd) = NULL;

static void ensure_resolved(void) {
    if (!orig_open) orig_open = dlsym(RTLD_NEXT, "open");
    if (!orig_openat) orig_openat = dlsym(RTLD_NEXT, "openat");
    if (!orig_read) orig_read = dlsym(RTLD_NEXT, "read");
    if (!orig_write) orig_write = dlsym(RTLD_NEXT, "write");
    if (!orig_close) orig_close = dlsym(RTLD_NEXT, "close");
}

// helper to print flags (simple)
static void print_open_flags(int flags) {
    int first = 1;
    if ((flags & O_RDONLY) == O_RDONLY) { fprintf(stderr, "O_RDONLY"); first = 0; }
    if (flags & O_WRONLY) { if (!first) fprintf(stderr, "|"); fprintf(stderr, "O_WRONLY"); first = 0; }
    if (flags & O_RDWR)   { if (!first) fprintf(stderr, "|"); fprintf(stderr, "O_RDWR"); first = 0; }
    if (flags & O_CREAT)  { if (!first) fprintf(stderr, "|"); fprintf(stderr, "O_CREAT"); first = 0; }
    if (flags & O_TRUNC)  { if (!first) fprintf(stderr, "|"); fprintf(stderr, "O_TRUNC"); first = 0; }
    if (flags & O_APPEND) { if (!first) fprintf(stderr, "|"); fprintf(stderr, "O_APPEND"); first = 0; }
    if (first) fprintf(stderr, "0");
}

// ═════════════════════════════════════════════════════════════
// open()
// ═════════════════════════════════════════════════════════════
int open(const char *pathname, int flags, ...) {
    ensure_resolved();

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int res;
    if (flags & O_CREAT) {
        res = orig_open(pathname, flags, mode);
    } else {
        res = orig_open(pathname, flags);
    }

    fprintf(stderr, "[SPY] open(\"%s\", ", pathname ? pathname : "(NULL)");
    print_open_flags(flags);
    if (flags & O_CREAT) fprintf(stderr, ", mode=0%o", mode);
    fprintf(stderr, ") = %d (%s)\n", res, (res == -1) ? strerror(errno) : "OK");

    return res;
}

// ═════════════════════════════════════════════════════════════
// openat()
// ═════════════════════════════════════════════════════════════
int openat(int dirfd, const char *pathname, int flags, ...) {
    ensure_resolved();

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int res;
    if (flags & O_CREAT) {
        res = orig_openat(dirfd, pathname, flags, mode);
    } else {
        res = orig_openat(dirfd, pathname, flags);
    }

    const char *dirfd_s = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "fd";
    fprintf(stderr, "[SPY] openat(%s, \"%s\", ", dirfd_s, pathname ? pathname : "(NULL)");
    print_open_flags(flags);
    if (flags & O_CREAT) fprintf(stderr, ", mode=0%o", mode);
    fprintf(stderr, ") = %d (%s)\n", res, (res == -1) ? strerror(errno) : "OK");

    return res;
}

// ═════════════════════════════════════════════════════════════
// read()
// ═════════════════════════════════════════════════════════════
ssize_t read(int fd, void *buf, size_t count) {
    ensure_resolved();
    ssize_t res = orig_read(fd, buf, count);
    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd (%s)\n",
            fd, buf, count, res, (res == -1) ? strerror(errno) : "OK");
    return res;
}

// ═════════════════════════════════════════════════════════════
// write()
// ═════════════════════════════════════════════════════════════
ssize_t write(int fd, const void *buf, size_t count) {
    ensure_resolved();
    // Avoid logging writes to stderr (fd == 2) to prevent recursion
    ssize_t res = orig_write(fd, buf, count);
    if (fd != 2) {
        // If it's stdout and small, print contents too (guarded)
        if (fd == 1 && count > 0 && count < 200) {
            // print as string but avoid non-printable explosion
            fprintf(stderr, "[SPY] write(fd=%d, \"%.*s\", count=%zu) = %zd (%s)\n",
                    fd, (int)count, (const char*)buf, count, res, (res == -1) ? strerror(errno) : "OK");
        } else {
            fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd (%s)\n",
                    fd, buf, count, res, (res == -1) ? strerror(errno) : "OK");
        }
    }
    return res;
}

// ═════════════════════════════════════════════════════════════
// close()
// ═════════════════════════════════════════════════════════════
int close(int fd) {
    ensure_resolved();
    int res = orig_close(fd);
    fprintf(stderr, "[SPY] close(fd=%d) = %d (%s)\n", fd, res, (res == -1) ? strerror(errno) : "OK");
    return res;
}
