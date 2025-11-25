#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


void print_open_flags(int flags) {
    fprintf(stderr, "(");
    if (flags & O_RDONLY) fprintf(stderr, "O_RDONLY|");
    if (flags & O_WRONLY) fprintf(stderr, "O_WRONLY|");
    if (flags & O_RDWR) fprintf(stderr, "O_RDWR|");
    if (flags & O_CREAT) fprintf(stderr, "O_CREAT|");
    if (flags & O_EXCL) fprintf(stderr, "O_EXCL|");
    if (flags & O_TRUNC) fprintf(stderr, "O_TRUNC|");
    if (flags & O_APPEND) fprintf(stderr, "O_APPEND|");
    if (flags & O_DIRECTORY) fprintf(stderr, "O_DIRECTORY|");
    if (flags & O_CLOEXEC) fprintf(stderr, "O_CLOEXEC|");
    if (flags & O_PATH) fprintf(stderr, "O_PATH|");
    
    long pos = ftell(stderr);
    fseek(stderr, pos - 1, SEEK_SET);
    fprintf(stderr, ")");
    fseek(stderr, 0, SEEK_END);
}

const char* get_error_name(int err) {
    switch(err) {
        case EACCES: return "EACCES";
        case ENOENT: return "ENOENT";
        case EEXIST: return "EEXIST";
        case EISDIR: return "EISDIR";
        case EINVAL: return "EINVAL";
        case EBADF: return "EBADF";
        default: return "UNKNOWN";
    }
}

// Перехват open()

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
    
    fprintf(stderr, "[SPY] open(path=\"%s\", flags=0x%x", pathname, flags);
    print_open_flags(flags);
    if (flags & O_CREAT) {
        fprintf(stderr, ", mode=0%03o", mode);
    }
    
    if (result < 0) {
        fprintf(stderr, ") = -1 (%s)\n", get_error_name(errno));
    } else {
        fprintf(stderr, ") = %d\n", result);
    }
    
    return result;
}

// Перехват openat()

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
    
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
    fprintf(stderr, "[SPY] openat(dirfd=%s, path=\"%s\", flags=0x%x", 
            dirfd_str, pathname, flags);
    print_open_flags(flags);
    if (flags & O_CREAT) {
        fprintf(stderr, ", mode=0%03o", mode);
    }
    
    if (result < 0) {
        fprintf(stderr, ") = -1 (%s)\n", get_error_name(errno));
    } else {
        fprintf(stderr, ") = %d\n", result);
    }
    
    return result;
}

// Перехват read()

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
    }

    ssize_t result = original_read(fd, buf, count);
    
    if (result < 0) {
        fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = -1 (%s)\n", 
                fd, buf, count, get_error_name(errno));
    } else {
        fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd bytes\n", 
                fd, buf, count, result);
        
        if (result > 0 && result < 100 && fd != 2) {
            fprintf(stderr, "[SPY]   data: \"%.*s\"\n", (int)result, (char*)buf);
        }
    }
    
    return result;
}

// Перехват write()

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
    }

    ssize_t result = original_write(fd, buf, count);
    
    if (fd != 2) {
        if (result < 0) {
            fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = -1 (%s)\n", 
                    fd, buf, count, get_error_name(errno));
        } else {
            fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd bytes\n", 
                    fd, buf, count, result);
            
            if (result > 0 && result < 100 && fd == 1) {
                fprintf(stderr, "[SPY]   data: \"%.*s\"\n", (int)result, (char*)buf);
            }
        }
    }
    
    return result;
}

// Перехват close()

int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
    }

    int result = original_close(fd);
    
    if (result < 0) {
        fprintf(stderr, "[SPY] close(fd=%d) = -1 (%s)\n", fd, get_error_name(errno));
    } else {
        fprintf(stderr, "[SPY] close(fd=%d) = 0\n", fd);
    }
    
    return result;
}
