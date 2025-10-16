// libsyscall_spy: перехват open/openat/read/write/close
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>

// Пишем в stderr, потокобезопасно
static pthread_mutex_t spy_lock = PTHREAD_MUTEX_INITIALIZER;

static void safe_log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    pthread_mutex_lock(&spy_lock);
    vfprintf(stderr, fmt, ap);
    fflush(stderr);
    pthread_mutex_unlock(&spy_lock);
    va_end(ap);
}

/* open */
int open(const char *pathname, int flags, ...) {
    static int (*orig_open)(const char*, int, ...) = NULL;
    if (!orig_open) orig_open = dlsym(RTLD_NEXT, "open");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int res = orig_open(pathname, flags, mode);
    safe_log("[SPY] open(\"%s\", flags=0x%x, mode=0%o) = %d (%s)\n",
             pathname ? pathname : "(null)", flags, mode, res, res==-1?strerror(errno):"OK");
    return res;
}

/* openat */
int openat(int dirfd, const char *pathname, int flags, ...) {
    static int (*orig_openat)(int,const char*,int,...) = NULL;
    if (!orig_openat) orig_openat = dlsym(RTLD_NEXT, "openat");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int res = orig_openat(dirfd, pathname, flags, mode);
    const char *df = (dirfd==AT_FDCWD) ? "AT_FDCWD" : "fd";
    safe_log("[SPY] openat(%s, \"%s\", 0x%x, mode=0%o) = %d (%s)\n",
             df, pathname?pathname:"(null)", flags, mode, res, res==-1?strerror(errno):"OK");
    return res;
}

/* read */
ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*orig_read)(int,void*,size_t) = NULL;
    if (!orig_read) orig_read = dlsym(RTLD_NEXT, "read");
    ssize_t res = orig_read(fd, buf, count);
    safe_log("[SPY] read(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, res);
    return res;
}

/* write */
ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*orig_write)(int,const void*,size_t) = NULL;
    if (!orig_write) orig_write = dlsym(RTLD_NEXT, "write");
    ssize_t res = orig_write(fd, buf, count);
    // Не логируем запись в stderr (fd==2) чтобы не зациклиться
    if (fd != 2) {
        safe_log("[SPY] write(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, res);
    }
    return res;
}

/* close */
int close(int fd) {
    static int (*orig_close)(int) = NULL;
    if (!orig_close) orig_close = dlsym(RTLD_NEXT, "close");
    int res = orig_close(fd);
    safe_log("[SPY] close(fd=%d) = %d (%s)\n", fd, res, res==-1?strerror(errno):"OK");
    return res;
}
