#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

extern "C" {

static int (*orig_open)(const char*, int, ...) = nullptr;
static int (*orig_openat)(int, const char*, int, ...) = nullptr;
static ssize_t (*orig_read)(int, void*, size_t) = nullptr;
static ssize_t (*orig_write)(int, const void*, size_t) = nullptr;
static int (*orig_close)(int) = nullptr;

static void resolve_functions_once() {
    if (!orig_open)    orig_open    = (int (*)(const char*, int, ...))dlsym(RTLD_NEXT, "open");
    if (!orig_openat)  orig_openat  = (int (*)(int, const char*, int, ...))dlsym(RTLD_NEXT, "openat");
    if (!orig_read)    orig_read    = (ssize_t (*)(int, void*, size_t))dlsym(RTLD_NEXT, "read");
    if (!orig_write)   orig_write   = (ssize_t (*)(int, const void*, size_t))dlsym(RTLD_NEXT, "write");
    if (!orig_close)   orig_close   = (int (*)(int))dlsym(RTLD_NEXT, "close");
}

static void print_open_flags(int flags, char *buf, size_t buflen) {
    buf[0] = '\0';
    bool first = true;
    #define APPEND_FLAG(s) do { if (!first) strncat(buf, "|", buflen - strlen(buf) - 1); strncat(buf, s, buflen - strlen(buf) - 1); first = false; } while(0)
    if ((flags & O_RDONLY) == O_RDONLY) APPEND_FLAG("O_RDONLY");
    if ((flags & O_WRONLY) == O_WRONLY) APPEND_FLAG("O_WRONLY");
    if ((flags & O_RDWR)   == O_RDWR)   APPEND_FLAG("O_RDWR");
    if (flags & O_CREAT)  APPEND_FLAG("O_CREAT");
    if (flags & O_TRUNC)  APPEND_FLAG("O_TRUNC");
    if (flags & O_APPEND) APPEND_FLAG("O_APPEND");
    if (flags & O_CLOEXEC) APPEND_FLAG("O_CLOEXEC");
    #undef APPEND_FLAG
    if (buf[0] == '\0') strncpy(buf, "0", buflen-1);
}

int open(const char *pathname, int flags, ...) {
    resolve_functions_once();
    if (!orig_open) {
        errno = ENOSYS;
        return -1;
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int res = orig_open(pathname, flags, mode);

    char flagsbuf[256];
    print_open_flags(flags, flagsbuf, sizeof(flagsbuf));

    if (flags & O_CREAT) {
        fprintf(stderr, "[SPY] open(\"%s\", %s, mode=0%o) = %d\n",
                pathname ? pathname : "(null)", flagsbuf, mode, res);
    } else {
        fprintf(stderr, "[SPY] open(\"%s\", %s) = %d\n",
                pathname ? pathname : "(null)", flagsbuf, res);
    }

    return res;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    resolve_functions_once();
    if (!orig_openat) {
        errno = ENOSYS;
        return -1;
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int res = orig_openat(dirfd, pathname, flags, mode);

    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "fd";
    char flagsbuf[256];
    print_open_flags(flags, flagsbuf, sizeof(flagsbuf));

    if (flags & O_CREAT) {
        fprintf(stderr, "[SPY] openat(%s, \"%s\", %s, mode=0%o) = %d\n",
                dirfd_str, pathname ? pathname : "(null)", flagsbuf, mode, res);
    } else {
        fprintf(stderr, "[SPY] openat(%s, \"%s\", %s) = %d\n",
                dirfd_str, pathname ? pathname : "(null)", flagsbuf, res);
    }

    return res;
}

ssize_t read(int fd, void *buf, size_t count) {
    resolve_functions_once();
    if (!orig_read) {
        errno = ENOSYS;
        return -1;
    }

    ssize_t res = orig_read(fd, buf, count);

    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd\n",
            fd, buf, count, res);

    return res;
}

ssize_t write(int fd, const void *buf, size_t count) {
    resolve_functions_once();
    if (!orig_write) {
        errno = ENOSYS;
        return -1;
    }

    ssize_t res = orig_write(fd, buf, count);

    if (fd != 2) {
        if ((fd == 1 || fd == STDOUT_FILENO) && count > 0 && count < 200) {
            size_t n = (count < 199) ? count : 199;
            char tmp[200];
            memcpy(tmp, buf, n);
            tmp[n] = '\0';
            fprintf(stderr, "[SPY] write(fd=%d, buf=\"%s\", count=%zu) = %zd\n",
                    fd, tmp, count, res);
        } else {
            fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd\n",
                    fd, buf, count, res);
        }
    }

    return res;
}

int close(int fd) {
    resolve_functions_once();
    if (!orig_close) {
        errno = ENOSYS;
        return -1;
    }

    int res = orig_close(fd);

    fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, res);

    return res;
}

}
