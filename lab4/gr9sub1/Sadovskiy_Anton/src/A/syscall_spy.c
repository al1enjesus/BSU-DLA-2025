#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>

#undef open
#undef openat
#undef read
#undef write
#undef close
#undef send
#undef recv

static __thread int in_log = 0;

// ──────────────────────────────
// Печать флагов open()
// ──────────────────────────────
static void print_open_flags(int flags)
{
    fprintf(stderr, "flags=");
    if (flags & O_RDONLY)
        fprintf(stderr, "O_RDONLY|");
    if (flags & O_WRONLY)
        fprintf(stderr, "O_WRONLY|");
    if (flags & O_RDWR)
        fprintf(stderr, "O_RDWR|");
    if (flags & O_CREAT)
        fprintf(stderr, "O_CREAT|");
    if (flags & O_TRUNC)
        fprintf(stderr, "O_TRUNC|");
    if (flags & O_APPEND)
        fprintf(stderr, "O_APPEND|");
    fprintf(stderr, "0x%x", flags);
}

// ──────────────────────────────
int open(const char *pathname, int flags, ...)
{
    static int (*orig_open)(const char *, int, ...) = NULL;
    if (!orig_open)
        orig_open = dlsym(RTLD_NEXT, "open");
    mode_t mode = 0;
    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    int res = orig_open(pathname, flags, mode);
    if (!in_log)
    {
        in_log = 1;
        fprintf(stderr, "[SPY] open(\"%s\", ", pathname);
        print_open_flags(flags);
        fprintf(stderr, ") = %d (%s)\n", res, (res == -1 ? strerror(errno) : "OK"));
        in_log = 0;
    }
    return res;
}

int openat(int dirfd, const char *pathname, int flags, ...)
{
    static int (*orig_openat)(int, const char *, int, ...) = NULL;
    if (!orig_openat)
        orig_openat = dlsym(RTLD_NEXT, "openat");
    mode_t mode = 0;
    if (flags & O_CREAT)
    {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    int res = orig_openat(dirfd, pathname, flags, mode);
    if (!in_log)
    {
        in_log = 1;
        fprintf(stderr, "[SPY] openat(%d, \"%s\", 0x%x) = %d\n", dirfd, pathname, flags, res);
        in_log = 0;
    }
    return res;
}

ssize_t read(int fd, void *buf, size_t count)
{
    static ssize_t (*orig_read)(int, void *, size_t) = NULL;
    if (!orig_read)
        orig_read = dlsym(RTLD_NEXT, "read");
    ssize_t res = orig_read(fd, buf, count);
    if (!in_log)
    {
        in_log = 1;
        fprintf(stderr, "[SPY] read(fd=%d, count=%zu) = %zd\n", fd, count, res);
        in_log = 0;
    }
    return res;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    static ssize_t (*orig_write)(int, const void *, size_t) = NULL;
    if (!orig_write)
        orig_write = dlsym(RTLD_NEXT, "write");
    ssize_t res = orig_write(fd, buf, count);
    if (!in_log)
    {
        in_log = 1;
        fprintf(stderr, "[SPY] write(fd=%d, count=%zu) = %zd\n", fd, count, res);
        in_log = 0;
    }
    return res;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    static ssize_t (*orig_send)(int, const void *, size_t, int) = NULL;
    if (!orig_send)
        orig_send = dlsym(RTLD_NEXT, "send");
    ssize_t res = orig_send(sockfd, buf, len, flags);
    if (!in_log)
    {
        in_log = 1;
        fprintf(stderr, "[SPY] send(fd=%d, len=%zu, flags=0x%x) = %zd\n", sockfd, len, flags, res);
        in_log = 0;
    }
    return res;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    static ssize_t (*orig_recv)(int, void *, size_t, int) = NULL;
    if (!orig_recv)
        orig_recv = dlsym(RTLD_NEXT, "recv");
    ssize_t res = orig_recv(sockfd, buf, len, flags);
    if (!in_log)
    {
        in_log = 1;
        fprintf(stderr, "[SPY] recv(fd=%d, len=%zu, flags=0x%x) = %zd\n", sockfd, len, flags, res);
        in_log = 0;
    }
    return res;
}

int close(int fd)
{
    static int (*orig_close)(int) = NULL;
    if (!orig_close)
        orig_close = dlsym(RTLD_NEXT, "close");
    int res = orig_close(fd);
    if (!in_log)
    {
        in_log = 1;
        fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, res);
        in_log = 0;
    }
    return res;
}
