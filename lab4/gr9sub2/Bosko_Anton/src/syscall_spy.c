#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

static void spy_log(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        syscall(SYS_write, STDERR_FILENO, buf, (size_t)n);
    }
}

static void decode_open_flags(int flags, char *out, size_t outlen) {
    out[0] = '\0';
    if ((flags & O_RDONLY) == O_RDONLY) strncat(out, "O_RDONLY|", outlen-1);
    if ((flags & O_WRONLY) == O_WRONLY) strncat(out, "O_WRONLY|", outlen-1);
    if ((flags & O_RDWR)   == O_RDWR)   strncat(out, "O_RDWR|", outlen-1);
    if (flags & O_CREAT)  strncat(out, "O_CREAT|", outlen-1);
    if (flags & O_TRUNC)  strncat(out, "O_TRUNC|", outlen-1);
    if (flags & O_APPEND) strncat(out, "O_APPEND|", outlen-1);
    if (strlen(out) == 0) strncat(out, "0", outlen-1);
}

int open(const char *pathname, int flags, ...) {
    static int (*orig_open)(const char*, int, ...) = NULL;
    if (!orig_open) {
        orig_open = (void*) dlsym(RTLD_NEXT, "open");
        if (!orig_open) {
            spy_log("[SPY] dlsym(open) failed: %s\n", dlerror());
            errno = ENOSYS;
            return -1;
        }
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int res = orig_open(pathname, flags, mode);

    char flags_str[256];
    decode_open_flags(flags, flags_str, sizeof(flags_str));

    spy_log("[SPY] open(\"%s\", flags=0x%x(%s), mode=0%o) = %d (errno=%d)\n",
            pathname ? pathname : "(null)", flags, flags_str, mode, res, errno);

    return res;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    static int (*orig_openat)(int, const char*, int, ...) = NULL;
    if (!orig_openat) {
        orig_openat = (void*) dlsym(RTLD_NEXT, "openat");
        if (!orig_openat) {
            spy_log("[SPY] dlsym(openat) failed: %s\n", dlerror());
            errno = ENOSYS;
            return -1;
        }
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int res = orig_openat(dirfd, pathname, flags, mode);

    char flags_str[256];
    decode_open_flags(flags, flags_str, sizeof(flags_str));
    const char *dfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "fd";

    spy_log("[SPY] openat(%s, \"%s\", 0x%x(%s), mode=0%o) = %d (errno=%d)\n",
            dfd_str, pathname ? pathname : "(null)", flags, flags_str, mode, res, errno);

    return res;
}

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*orig_read)(int, void*, size_t) = NULL;
    if (!orig_read) {
        orig_read = (void*) dlsym(RTLD_NEXT, "read");
        if (!orig_read) {
            spy_log("[SPY] dlsym(read) failed: %s\n", dlerror());
            errno = ENOSYS;
            return -1;
        }
    }

    ssize_t res = orig_read(fd, buf, count);

    spy_log("[SPY] read(fd=%d, buf=%p, count=%zu) = %zd (errno=%d)\n",
            fd, buf, count, res, errno);

    return res;
}

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*orig_write)(int, const void*, size_t) = NULL;
    if (!orig_write) {
        orig_write = (void*) dlsym(RTLD_NEXT, "write");
        if (!orig_write) {
            ssize_t res = syscall(SYS_write, fd, buf, count);
            spy_log("[SPY] write(dlsym_failed) fd=%d, count=%zu => %zd\n", fd, count, res);
            return res;
        }
    }

    ssize_t res = orig_write(fd, buf, count);

    if (fd != STDERR_FILENO) {
        spy_log("[SPY] write(fd=%d, buf=%p, count=%zu) = %zd (errno=%d)\n",
                fd, buf, count, res, errno);
    }

    return res;
}

int close(int fd) {
    static int (*orig_close)(int) = NULL;
    if (!orig_close) {
        orig_close = (void*) dlsym(RTLD_NEXT, "close");
        if (!orig_close) {
            spy_log("[SPY] dlsym(close) failed: %s\n", dlerror());
            errno = ENOSYS;
            return -1;
        }
    }

    int res = orig_close(fd);

    spy_log("[SPY] close(fd=%d) = %d (errno=%d)\n", fd, res, errno);

    return res;
}
