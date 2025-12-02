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

// Таблица сопоставления файловых дескрипторов и путей
static const char *fd_map_path[MAX_MAP] = {0};

// Указатели на оригинальные функции libc
static int (*orig_open)(const char *, int, ...) = NULL;
static int (*orig_openat)(int, const char *, int, ...) = NULL;
static ssize_t (*orig_read)(int, void *, size_t) = NULL;
static ssize_t (*orig_write)(int, const void *, size_t) = NULL;
static int (*orig_close)(int) = NULL;

// Инициализация указателей на оригинальные функции
static void init_originals(void) {
    if (!orig_open)     orig_open     = dlsym(RTLD_NEXT, "open");
    if (!orig_openat)   orig_openat   = dlsym(RTLD_NEXT, "openat");
    if (!orig_read)     orig_read     = dlsym(RTLD_NEXT, "read");
    if (!orig_write)    orig_write    = dlsym(RTLD_NEXT, "write");
    if (!orig_close)    orig_close    = dlsym(RTLD_NEXT, "close");
}

// Печать временной метки, PID и TID
static void print_header(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    pid_t pid = getpid();
    long tid = syscall(SYS_gettid);
    fprintf(stderr, "[%ld.%03ld] pid=%d tid=%ld ",
            ts.tv_sec, ts.tv_nsec / 1000000, pid, tid);
}

// Работа с таблицей путей
static void map_fd_set(int fd, const char *path) {
    if (fd >= 0 && fd < MAX_MAP) {
        if (fd_map_path[fd]) free((void *)fd_map_path[fd]);
        fd_map_path[fd] = path ? strdup(path) : NULL;
    }
}

static const char *map_fd_get(int fd) {
    if (fd >= 0 && fd < MAX_MAP)
        return fd_map_path[fd];
    return NULL;
}

static void map_fd_unset(int fd) {
    if (fd >= 0 && fd < MAX_MAP) {
        free((void *)fd_map_path[fd]);
        fd_map_path[fd] = NULL;
    }
}

// Перехват open()
int open(const char *pathname, int flags, ...) {
    init_originals();

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int fd = orig_open(pathname, flags, mode);
    print_header();

    if (fd == -1) {
        int saved_errno = errno;
        fprintf(stderr, "[SPY] open(\"%s\", 0x%x) = %d errno=%d (%s)\n",
                pathname ?: "NULL", flags, fd, saved_errno, strerror(saved_errno));
    } else {
        fprintf(stderr, "[SPY] open(\"%s\", 0x%x) = %d\n",
                pathname ?: "NULL", flags, fd);
        map_fd_set(fd, pathname);
    }

    return fd;
}

// Перехват openat()
int openat(int dirfd, const char *pathname, int flags, ...) {
    init_originals();

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int fd = orig_openat(dirfd, pathname, flags, mode);
    print_header();

    if (fd == -1) {
        int saved_errno = errno;
        fprintf(stderr, "[SPY] openat(%d, \"%s\", 0x%x) = %d errno=%d (%s)\n",
                dirfd, pathname ?: "NULL", flags, fd, saved_errno, strerror(saved_errno));
    } else {
        fprintf(stderr, "[SPY] openat(%d, \"%s\", 0x%x) = %d\n",
                dirfd, pathname ?: "NULL", flags, fd);
        map_fd_set(fd, pathname);
    }

    return fd;
}

// Перехват read()
ssize_t read(int fd, void *buf, size_t count) {
    init_originals();
    ssize_t ret = orig_read(fd, buf, count);
    print_header();

    const char *path = map_fd_get(fd);
    if (ret == -1) {
        int saved_errno = errno;
        fprintf(stderr, "[SPY] read(fd=%d%s, count=%zu) = %zd errno=%d (%s)\n",
                fd, path ? path : "", count, ret, saved_errno, strerror(saved_errno));
    } else {
        fprintf(stderr, "[SPY] read(fd=%d%s, count=%zu) = %zd\n",
                fd, path ? path : "", count, ret);
    }

    return ret;
}

// Перехват write()
ssize_t write(int fd, const void *buf, size_t count) {
    init_originals();

    // Не перехватываем запись в stderr (fd==2)
    if (fd == STDERR_FILENO)
        return orig_write(fd, buf, count);

    ssize_t ret = orig_write(fd, buf, count);
    print_header();

    const char *path = map_fd_get(fd);
    if (ret == -1) {
        int saved_errno = errno;
        fprintf(stderr, "[SPY] write(fd=%d%s, count=%zu) = %zd errno=%d (%s)\n",
                fd, path ? path : "", count, ret, saved_errno, strerror(saved_errno));
    } else {
        fprintf(stderr, "[SPY] write(fd=%d%s, count=%zu) = %zd\n",
                fd, path ? path : "", count, ret);
    }

    return ret;
}

// Перехват close()
int close(int fd) {
    init_originals();
    int ret = orig_close(fd);
    print_header();

    const char *path = map_fd_get(fd);
    if (ret == -1) {
        int saved_errno = errno;
        fprintf(stderr, "[SPY] close(fd=%d%s) = %d errno=%d (%s)\n",
                fd, path ? path : "", ret, saved_errno, strerror(saved_errno));
    } else {
        fprintf(stderr, "[SPY] close(fd=%d%s) = %d\n", fd, path ? path : "", ret);
        map_fd_unset(fd);
    }

    return ret;
}