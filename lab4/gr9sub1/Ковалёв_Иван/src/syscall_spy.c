#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// ==================== ДЕКОДИРОВАНИЕ ФЛАГОВ OPEN() ====================

const char* decode_open_flags(int flags) {
    static char buffer[256];
    buffer[0] = '\0';
    
    if (flags & O_RDONLY) strcat(buffer, "O_RDONLY|");
    if (flags & O_WRONLY) strcat(buffer, "O_WRONLY|");
    if (flags & O_RDWR)   strcat(buffer, "O_RDWR|");
    if (flags & O_CREAT)  strcat(buffer, "O_CREAT|");
    if (flags & O_EXCL)   strcat(buffer, "O_EXCL|");
    if (flags & O_TRUNC)  strcat(buffer, "O_TRUNC|");
    if (flags & O_APPEND) strcat(buffer, "O_APPEND|");
    if (flags & O_DIRECTORY) strcat(buffer, "O_DIRECTORY|");
    
    // Убираем последний символ '|'
    size_t len = strlen(buffer);
    if (len > 0) buffer[len-1] = '\0';
    
    return buffer;
}

const char* decode_dirfd(int dirfd) {
    if (dirfd == AT_FDCWD) return "AT_FDCWD";
    
    static char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", dirfd);
    return buffer;
}

// ==================== ПЕРЕХВАТ OPEN() ====================

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

    const char* flags_str = decode_open_flags(flags);
    fprintf(stderr, "[SPY] open(\"%s\", %s, 0%o) = %d", 
            pathname, flags_str, mode, result);
    
    if (result == -1) {
        fprintf(stderr, " [ERROR: %s]", strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}

// ==================== ПЕРЕХВАТ OPENAT() ====================

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

    const char* dirfd_str = decode_dirfd(dirfd);
    const char* flags_str = decode_open_flags(flags);
    
    fprintf(stderr, "[SPY] openat(%s, \"%s\", %s, 0%o) = %d", 
            dirfd_str, pathname, flags_str, mode, result);
    
    if (result == -1) {
        fprintf(stderr, " [ERROR: %s]", strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}

// ==================== ПЕРЕХВАТ READ() ====================

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
    }

    ssize_t result = original_read(fd, buf, count);

    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd", 
            fd, buf, count, result);
    
    if (result == -1) {
        fprintf(stderr, " [ERROR: %s]", strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}

// ==================== ПЕРЕХВАТ WRITE() С ВЫВОДОМ СОДЕРЖИМОГО ====================

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
    }

    ssize_t result = original_write(fd, buf, count);

    // Избегаем рекурсии - не логируем запись в stderr (fd=2)
    if (fd != 2) {
        fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd", 
                fd, buf, count, result);
        
        // Выводим содержимое для небольших записей в stdout (fd=1)
        if (fd == 1 && count > 0 && count <= 100) {
            fprintf(stderr, " DATA: \"");
            // Безопасный вывод - заменяем непечатаемые символы
            for (size_t i = 0; i < count && i < 50; i++) {
                char c = ((char*)buf)[i];
                if (c >= 32 && c <= 126) {
                    fputc(c, stderr);
                } else {
                    fprintf(stderr, "\\x%02x", (unsigned char)c);
                }
            }
            if (count > 50) fprintf(stderr, "...");
            fprintf(stderr, "\"");
        }
        
        if (result == -1) {
            fprintf(stderr, " [ERROR: %s]", strerror(errno));
        }
        fprintf(stderr, "\n");
    }

    return result;
}

// ==================== ПЕРЕХВАТ CLOSE() ====================

int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
    }

    int result = original_close(fd);

    fprintf(stderr, "[SPY] close(fd=%d) = %d", fd, result);
    
    if (result == -1) {
        fprintf(stderr, " [ERROR: %s]", strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}