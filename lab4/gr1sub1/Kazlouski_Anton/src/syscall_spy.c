#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>

void print_open_flags(int flags, char* buffer, size_t size) {
    buffer[0] = '\0';
    if (flags & O_RDONLY) strncat(buffer, "O_RDONLY|", size - strlen(buffer) - 1);
    if (flags & O_WRONLY) strncat(buffer, "O_WRONLY|", size - strlen(buffer) - 1);
    if (flags & O_RDWR) strncat(buffer, "O_RDWR|", size - strlen(buffer) - 1);
    if (flags & O_CREAT) strncat(buffer, "O_CREAT|", size - strlen(buffer) - 1);
    if (flags & O_TRUNC) strncat(buffer, "O_TRUNC|", size - strlen(buffer) - 1);
    if (flags & O_APPEND) strncat(buffer, "O_APPEND|", size - strlen(buffer) - 1);
    if (flags & O_EXCL) strncat(buffer, "O_EXCL|", size - strlen(buffer) - 1);

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '|') {
        buffer[len-1] = '\0';
    }
}

int open(const char *pathname, int flags, ...) {
    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) {
        original_open = dlsym(RTLD_NEXT, "open");
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = original_open(pathname, flags, mode);

    char flags_str[256];
    print_open_flags(flags, flags_str, sizeof(flags_str));
    fprintf(stderr, "[SPY] open(\"%s\", %s) = %d\n", pathname, flags_str, result);

    return result;
}

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

    char flags_str[256];
    print_open_flags(flags, flags_str, sizeof(flags_str));
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
    fprintf(stderr, "[SPY] openat(%s, \"%s\", %s) = %d\n",
            dirfd_str, pathname, flags_str, result);

    return result;
}

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

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
    }

    ssize_t result = original_write(fd, buf, count);

    if (fd != 2) {
        fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd\n",
                fd, buf, count, result);
    }

    return result;
}

int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
    }

    int result = original_close(fd);

    fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, result);

    return result;
}