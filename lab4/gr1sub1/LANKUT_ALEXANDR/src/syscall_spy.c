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
    logfile = stderr;
    fprintf(logfile, "[SPY] Logging initialized\n");
    fflush(logfile);
}

static void print_open_flags(FILE *f, int flags) {
    int first = 1;
    #define PFLAG(x) do { if (flags & (x)) { if(!first) fputc('|', f); fputs(#x,f); first=0; } } while(0)
    
    if ((flags & O_ACCMODE) == O_RDONLY) { fputs("O_RDONLY", f); first=0; }
    else if ((flags & O_ACCMODE) == O_WRONLY) { fputs("O_WRONLY", f); first=0; }
    else if ((flags & O_ACCMODE) == O_RDWR) { fputs("O_RDWR", f); first=0; }
    
    PFLAG(O_CREAT);
    PFLAG(O_EXCL);
    PFLAG(O_NOCTTY);
    PFLAG(O_TRUNC);
    PFLAG(O_APPEND);
    PFLAG(O_NONBLOCK);
    PFLAG(O_SYNC);
    PFLAG(O_ASYNC);
    PFLAG(O_CLOEXEC);
    PFLAG(O_DIRECTORY);
    #undef PFLAG
}

int open(const char *pathname, int flags, ...) {
    pthread_once(&init_once, init_log);

    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) {
        original_open = dlsym(RTLD_NEXT, "open");
        if (!original_open) {
            fprintf(logfile, "[SPY] Failed to get original open(): %s\n", dlerror());
            return -1;
        }
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = original_open(pathname, flags, mode);
    int saved_errno = errno;

    fprintf(logfile, "[SPY] open(\"%s\", ", pathname ? pathname : "(null)");
    print_open_flags(logfile, flags);
    if (flags & O_CREAT) fprintf(logfile, ", mode=0%o", mode);
    fprintf(logfile, ") = %d", result);
    if (result == -1) fprintf(logfile, " [%s]", strerror(saved_errno));
    fprintf(logfile, "\n");
    fflush(logfile);

    errno = saved_errno;
    return result;
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    pthread_once(&init_once, init_log);

    static int (*original_openat)(int, const char*, int, ...) = NULL;
    if (!original_openat) {
        original_openat = dlsym(RTLD_NEXT, "openat");
        if (!original_openat) {
            fprintf(logfile, "[SPY] Failed to get original openat(): %s\n", dlerror());
            return -1;
        }
    }

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = original_openat(dirfd, pathname, flags, mode);
    int saved_errno = errno;

    fprintf(logfile, "[SPY] openat(%s, \"%s\", ", 
            dirfd == AT_FDCWD ? "AT_FDCWD" : "fd",
            pathname ? pathname : "(null)");
    print_open_flags(logfile, flags);
    if (flags & O_CREAT) fprintf(logfile, ", mode=0%o", mode);
    fprintf(logfile, ") = %d", result);
    if (result == -1) fprintf(logfile, " [%s]", strerror(saved_errno));
    fprintf(logfile, "\n");
    fflush(logfile);

    errno = saved_errno;
    return result;
}

ssize_t read(int fd, void *buf, size_t count) {
    pthread_once(&init_once, init_log);

    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
        if (!original_read) {
            fprintf(logfile, "[SPY] Failed to get original read(): %s\n", dlerror());
            return -1;
        }
    }

    ssize_t result = original_read(fd, buf, count);
    int saved_errno = errno;

    fprintf(logfile, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd", 
            fd, buf, count, result);
    if (result == -1) fprintf(logfile, " [%s]", strerror(saved_errno));
    fprintf(logfile, "\n");
    fflush(logfile);

    errno = saved_errno;
    return result;
}

ssize_t write(int fd, const void *buf, size_t count) {
    pthread_once(&init_once, init_log);

    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
        if (!original_write) {
            fprintf(logfile, "[SPY] Failed to get original write(): %s\n", dlerror());
            return -1;
        }
    }

    int do_log = (fd != 2);
    
    if (do_log) {
        fprintf(logfile, "[SPY] write(fd=%d, buf=%p, count=%zu", fd, buf, count);
        if (fd == 1 && count < 100) {
            fprintf(logfile, ", \"%.*s\"", (int)count, (const char*)buf);
        }
        fprintf(logfile, ") = ");
        fflush(logfile);
    }

    ssize_t result = original_write(fd, buf, count);
    int saved_errno = errno;

    if (do_log) {
        fprintf(logfile, "%zd", result);
        if (result == -1) fprintf(logfile, " [%s]", strerror(saved_errno));
        fprintf(logfile, "\n");
        fflush(logfile);
    }

    errno = saved_errno;
    return result;
}

int close(int fd) {
    pthread_once(&init_once, init_log);

    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
        if (!original_close) {
            fprintf(logfile, "[SPY] Failed to get original close(): %s\n", dlerror());
            return -1;
        }
    }

    int result = original_close(fd);
    int saved_errno = errno;

    fprintf(logfile, "[SPY] close(fd=%d) = %d", fd, result);
    if (result == -1) fprintf(logfile, " [%s]", strerror(saved_errno));
    fprintf(logfile, "\n");
    fflush(logfile);

    errno = saved_errno;
    return result;
}