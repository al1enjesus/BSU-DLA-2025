#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

// Thread-local защита от рекурсии
static __thread int in_trace = 0;

// Функция для безопасного логирования бинарных данных
static void print_safe_data(FILE *stream, const void *data, size_t len, size_t max_len) {
    if (len == 0) {
        fputs("<empty>", stream);
        return;
    }
    
    if (len > max_len) len = max_len;
    
    const unsigned char *bytes = (const unsigned char *)data;
    fputc('"', stream);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = bytes[i];
        if (c >= 32 && c <= 126 && c != '\\' && c != '"') { // Printable ASCII
            fputc(c, stream);
        } else if (c == '\n') {
            fputs("\\n", stream);
        } else if (c == '\t') {
            fputs("\\t", stream);
        } else if (c == '\r') {
            fputs("\\r", stream);
        } else if (c == '\\') {
            fputs("\\\\", stream);
        } else if (c == '"') {
            fputs("\\\"", stream);
        } else {
            fprintf(stream, "\\x%02x", c);
        }
    }
    if (len < max_len) {
        fputc('"', stream);
    } else {
        fputs("\"...", stream); // Обрезанные данные
    }
}

// Функция для декодирования флагов open/openat
static void print_open_flags(FILE *stream, int flags) {
    const char *sep = "";
    if (flags & O_RDONLY) { fprintf(stream, "%sO_RDONLY", sep); sep = "|"; }
    if (flags & O_WRONLY) { fprintf(stream, "%sO_WRONLY", sep); sep = "|"; }
    if (flags & O_RDWR) { fprintf(stream, "%sO_RDWR", sep); sep = "|"; }
    if (flags & O_CREAT) { fprintf(stream, "%sO_CREAT", sep); sep = "|"; }
    if (flags & O_EXCL) { fprintf(stream, "%sO_EXCL", sep); sep = "|"; }
    if (flags & O_TRUNC) { fprintf(stream, "%sO_TRUNC", sep); sep = "|"; }
    if (flags & O_APPEND) { fprintf(stream, "%sO_APPEND", sep); sep = "|"; }
    if (flags & O_NONBLOCK) { fprintf(stream, "%sO_NONBLOCK", sep); sep = "|"; }
    if (flags & O_SYNC) { fprintf(stream, "%sO_SYNC", sep); sep = "|"; }
    if (flags & O_CLOEXEC) { fprintf(stream, "%sO_CLOEXEC", sep); sep = "|"; }
    if (flags & O_DIRECTORY) { fprintf(stream, "%sO_DIRECTORY", sep); sep = "|"; }
    if (flags & O_NOFOLLOW) { fprintf(stream, "%sO_NOFOLLOW", sep); sep = "|"; }
    if (*sep == 0) fprintf(stream, "0");
}

// Функция для логирования ошибок
static void print_error(FILE *stream) {
    int saved_errno = errno;
    fprintf(stream, " (errno=%d: %s)", saved_errno, strerror(saved_errno));
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
    
    if (!in_trace) {
        in_trace = 1;
        fprintf(stderr, "[SPY] open(\"%s\", flags=", pathname);
        print_open_flags(stderr, flags);
        if (flags & O_CREAT) {
            fprintf(stderr, ", mode=0%03o", mode);
        }
        fprintf(stderr, " [0x%x]) = %d", flags, result);
        if (result == -1) {
            print_error(stderr);
        }
        fprintf(stderr, "\n");
        in_trace = 0;
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
    
    if (!in_trace) {
        in_trace = 1;
        const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "<fd>";
        fprintf(stderr, "[SPY] openat(%s, \"%s\", flags=", dirfd_str, pathname);
        print_open_flags(stderr, flags);
        if (flags & O_CREAT) {
            fprintf(stderr, ", mode=0%03o", mode);
        }
        fprintf(stderr, " [0x%x]) = %d", flags, result);
        if (result == -1) {
            print_error(stderr);
        }
        fprintf(stderr, "\n");
        in_trace = 0;
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
    
    if (!in_trace) {
        in_trace = 1;
        fprintf(stderr, "[SPY] read(fd=%d, count=%zu) = %zd", fd, count, result);
        
        // Логируем данные только для успешных операций чтения
        if (result > 0) {
            fprintf(stderr, ", data=");
            print_safe_data(stderr, buf, result, 64); // Логируем до 64 байт
        }
        
        if (result == -1) {
            print_error(stderr);
        }
        fprintf(stderr, "\n");
        in_trace = 0;
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
    
    if (!in_trace && fd != 2) { // Избегаем рекурсии на stderr
        in_trace = 1;
        fprintf(stderr, "[SPY] write(fd=%d, count=%zu) = %zd", fd, count, result);
        
        // Логируем данные только для успешных операций записи
        if (result > 0) {
            fprintf(stderr, ", data=");
            print_safe_data(stderr, buf, result, 64); // Логируем до 64 байт
        }
        
        if (result == -1) {
            print_error(stderr);
        }
        fprintf(stderr, "\n");
        in_trace = 0;
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
    
    if (!in_trace) {
        in_trace = 1;
        fprintf(stderr, "[SPY] close(fd=%d) = %d", fd, result);
        if (result == -1) {
            print_error(stderr);
        }
        fprintf(stderr, "\n");
        in_trace = 0;
    }
    
    return result;
}
