// src/syscall_spy.c
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>

static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static FILE *logfile = NULL;

static void init_log(void) {
    // Логируем в stderr; можно заменить на файл fopen("logs/spy.log","a")
    logfile = stderr;
    fprintf(logfile, "[SPY] init_log() called\n");
    fflush(logfile);
}

static void print_open_flags(FILE *f, int flags) {
    int first = 1;
    #define PFLAG(x) do { if (flags & (x)) { if(!first) fputc('|', f); fputs(#x,f); first=0; } } while(0)
    if ((flags & O_ACCMODE) == O_RDONLY) { fputs("O_RDONLY", f); first=0;}
    else if ((flags & O_ACCMODE) == O_WRONLY) { fputs("O_WRONLY", f); first=0;}
    else if ((flags & O_ACCMODE) == O_RDWR) { fputs("O_RDWR", f); first=0;}
    PFLAG(O_CREAT);
    PFLAG(O_TRUNC);
    PFLAG(O_APPEND);
    PFLAG(O_NONBLOCK);
    PFLAG(O_SYNC);
    PFLAG(O_NOCTTY);
    #undef PFLAG
}

// Перехват open()
int open(const char *pathname, int flags, ...) {
    pthread_once(&init_once, init_log);

    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) original_open = dlsym(RTLD_NEXT, "open");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int res = original_open(pathname, flags, mode);
    int saved_errno = errno;

    fprintf(logfile, "[SPY] open(\"%s\", ", pathname ? pathname : "(null)");
    print_open_flags(logfile, flags);
    if (flags & O_CREAT) fprintf(logfile, ", mode=0%o", mode);
    fprintf(logfile, ") = %d (errno=%d)\n", res, saved_errno);
    fflush(logfile);

    errno = saved_errno;
    return res;
}

// Перехват openat()
int openat(int dirfd, const char *pathname, int flags, ...) {
    pthread_once(&init_once, init_log);

    static int (*original_openat)(int, const char*, int, ...) = NULL;
    if (!original_openat) original_openat = dlsym(RTLD_NEXT, "openat");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    int res = original_openat(dirfd, pathname, flags, mode);
    int saved_errno = errno;

    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "fd";
    fprintf(logfile, "[SPY] openat(%s, \"%s\", ", dirfd_str, pathname ? pathname : "(null)");
    print_open_flags(logfile, flags);
    if (flags & O_CREAT) fprintf(logfile, ", mode=0%o", mode);
    fprintf(logfile, ") = %d (errno=%d)\n", res, saved_errno);
    fflush(logfile);

    errno = saved_errno;
    return res;
}

// read()
ssize_t read(int fd, void *buf, size_t count) {
    pthread_once(&init_once, init_log);
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) original_read = dlsym(RTLD_NEXT, "read");

    ssize_t res = original_read(fd, buf, count);
    int saved_errno = errno;

    fprintf(logfile, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd (errno=%d)\n",
            fd, buf, count, res, saved_errno);

    // Не печатаем содержимое больших буферов; для маленьких можно добавить печать.
    fflush(logfile);
    errno = saved_errno;
    return res;
}

// write()
ssize_t write(int fd, const void *buf, size_t count) {
    pthread_once(&init_once, init_log);
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) original_write = dlsym(RTLD_NEXT, "write");

    // Предотвращаем рекурсию: не логируем write на stderr (fd==2) и на stdout (fd==1) если логтер пишет туда.
    int do_log = (fd != 2);
    ssize_t res = original_write(fd, buf, count);
    int saved_errno = errno;

    if (do_log) {
        if (fd == 1 && count < 256) {
            fprintf(logfile, "[SPY] write(fd=%d, \"%.*s\", %zu) = %zd (errno=%d)\n",
                    fd, (int)count, (const char*)buf, count, res, saved_errno);
        } else {
            fprintf(logfile, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd (errno=%d)\n",
                    fd, buf, count, res, saved_errno);
        }
        fflush(logfile);
    }

    errno = saved_errno;
    return res;
}

// close()
int close(int fd) {
    pthread_once(&init_once, init_log);
    static int (*original_close)(int) = NULL;
    if (!original_close) original_close = dlsym(RTLD_NEXT, "close");

    int res = original_close(fd);
    int saved_errno = errno;

    fprintf(logfile, "[SPY] close(fd=%d) = %d (errno=%d)\n", fd, res, saved_errno);
    fflush(logfile);

    errno = saved_errno;
    return res;
}
