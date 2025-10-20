#define _GNU_SOURCE
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/syscall.h>

#define MAX_MAP 1024
static const char *fd_map_path[MAX_MAP];

static int (*old_open)(const char *, int, ...) = NULL;
static int (*old_openat)(int, const char *, int, ...) = NULL;
static ssize_t (*old_read)(int, void *, size_t) = NULL;
static ssize_t (*old_write)(int, const void *, size_t) = NULL;
static int (*old_close)(int) = NULL;

static void init_funcs(void) {
    if (!old_open) old_open = dlsym(RTLD_NEXT, "open");
    if (!old_openat) old_openat = dlsym(RTLD_NEXT, "openat");
    if (!old_read) old_read = dlsym(RTLD_NEXT, "read");
    if (!old_write) old_write = dlsym(RTLD_NEXT, "write");
    if (!old_close) old_close = dlsym(RTLD_NEXT, "close");
}

static void print_header(void) {
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    pid_t pid = getpid();
    long tid = syscall(SYS_gettid);
    fprintf(stderr, "[%ld.%03ld] pid=%d tid=%ld ", t.tv_sec, t.tv_nsec/1000000, pid, tid);
}

static void map_fd_set(int fd, const char *s) {
    if (fd >= 0 && fd < MAX_MAP) {
        fd_map_path[fd] = s ? strdup(s) : NULL;
    }
}
static const char *map_fd_get(int fd) {
    if (fd >= 0 && fd < MAX_MAP) {
        return fd_map_path[fd];
    }
    return NULL;
}
static void map_fd_unset(int fd) {
    if (fd >= 0 && fd < MAX_MAP) {
        if (fd_map_path[fd]) { free((void*)fd_map_path[fd]); fd_map_path[fd] = NULL; }
    }
}

int open(const char *pathname, int flags, ...) {
    init_funcs();
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap; va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    int r = old_open(pathname, flags, mode);
    print_header();
    if (r == -1) {
        int e = errno;
        fprintf(stderr, "[SPY] open(\"%s\", 0x%x) = %d errno=%d (%s)\n", pathname?pathname:"NULL", flags, r, e, strerror(e));
    } else {
        fprintf(stderr, "[SPY] open(\"%s\", 0x%x) = %d\n", pathname?pathname:"NULL", flags, r);
        map_fd_set(r, pathname);
    }
    return r;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    init_funcs();
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap; va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    int r = old_openat(dirfd, pathname, flags, mode);
    print_header();
    if (r == -1) {
        int e = errno;
        fprintf(stderr, "[SPY] openat(%d, \"%s\", 0x%x) = %d errno=%d (%s)\n", dirfd, pathname?pathname:"NULL", flags, r, e, strerror(e));
    } else {
        fprintf(stderr, "[SPY] openat(%d, \"%s\", 0x%x) = %d\n", dirfd, pathname?pathname:"NULL", flags, r);
        map_fd_set(r, pathname);
    }
    return r;
}

ssize_t read(int fd, void *buf, size_t count) {
    init_funcs();
    ssize_t r = old_read(fd, buf, count);
    print_header();
    const char *m = map_fd_get(fd);
    if (r == -1) {
        int e = errno;
        fprintf(stderr, "[SPY] read(fd=%d%s, count=%zu) = %zd errno=%d (%s)\n",
                fd, m ? m : "", count, r, e, strerror(e));
    } else {
        fprintf(stderr, "[SPY] read(fd=%d%s, count=%zu) = %zd\n",
                fd, m ? m : "", count, r);
    }
    return r;
}

ssize_t write(int fd, const void *buf, size_t count) {
    init_funcs();
    if (fd == 2) return old_write(fd, buf, count);
    ssize_t r = old_write(fd, buf, count);
    print_header();
    const char *m = map_fd_get(fd);
    if (r == -1) {
        int e = errno;
        fprintf(stderr, "[SPY] write(fd=%d%s, count=%zu) = %zd errno=%d (%s)\n",
                fd, m ? m : "", count, r, e, strerror(e));
    } else {
        fprintf(stderr, "[SPY] write(fd=%d%s, count=%zu) = %zd\n",
                fd, m ? m : "", count, r);
    }
    return r;
}

int close(int fd) {
    init_funcs();
    int r = old_close(fd);
    print_header();
    const char *m = map_fd_get(fd);
    if (r == -1) {
        int e = errno;
        fprintf(stderr, "[SPY] close(fd=%d%s) = %d errno=%d (%s)\n", fd, m?m:"", r, e, strerror(e));
    } else {
        fprintf(stderr, "[SPY] close(fd=%d%s) = %d\n", fd, m?m:"", r);
        map_fd_unset(fd);
    }
    return r;
}
