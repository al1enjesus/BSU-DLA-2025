/**
 * syscall_spy.c - LD_PRELOAD library for intercepting system calls
 * Lab 4, Variant 19
 * 
 * Intercepts: open(), openat(), read(), write(), close()
 * Programs to analyze: gcc, make, as
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// Color codes for better readability
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"

// Statistics counters
static long open_calls = 0;
static long openat_calls = 0;
static long read_calls = 0;
static long write_calls = 0;
static long close_calls = 0;
static long total_bytes_read = 0;
static long total_bytes_written = 0;

// Constructor - runs when library is loaded
__attribute__((constructor))
void init() {
    fprintf(stderr, COLOR_BLUE "[SPY] ========== Syscall interception started ==========\n" COLOR_RESET);
}

// Destructor - runs when library is unloaded
__attribute__((destructor))
void fini() {
    fprintf(stderr, COLOR_BLUE "\n[SPY] ========== Statistics ==========\n");
    fprintf(stderr, "[SPY] open() calls:    %ld\n", open_calls);
    fprintf(stderr, "[SPY] openat() calls:  %ld\n", openat_calls);
    fprintf(stderr, "[SPY] read() calls:    %ld (total bytes: %ld)\n", read_calls, total_bytes_read);
    fprintf(stderr, "[SPY] write() calls:   %ld (total bytes: %ld)\n", write_calls, total_bytes_written);
    fprintf(stderr, "[SPY] close() calls:   %ld\n", close_calls);
    fprintf(stderr, "[SPY] ===================================\n" COLOR_RESET);
}

// Helper function to decode open flags
const char* decode_flags(int flags) {
    static char buffer[256];
    buffer[0] = '\0';
    
    if ((flags & O_ACCMODE) == O_RDONLY) strcat(buffer, "O_RDONLY|");
    if ((flags & O_ACCMODE) == O_WRONLY) strcat(buffer, "O_WRONLY|");
    if ((flags & O_ACCMODE) == O_RDWR)   strcat(buffer, "O_RDWR|");
    if (flags & O_CREAT)  strcat(buffer, "O_CREAT|");
    if (flags & O_EXCL)   strcat(buffer, "O_EXCL|");
    if (flags & O_TRUNC)  strcat(buffer, "O_TRUNC|");
    if (flags & O_APPEND) strcat(buffer, "O_APPEND|");
    if (flags & O_CLOEXEC) strcat(buffer, "O_CLOEXEC|");
    
    // Remove trailing pipe
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '|') buffer[len-1] = '\0';
    
    if (strlen(buffer) == 0) sprintf(buffer, "0x%x", flags);
    
    return buffer;
}

// =============== open() interception ===============
int open(const char *pathname, int flags, ...) __attribute__((nonnull (1)));
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
    
    open_calls++;
    
    if (result >= 0) {
        if (pathname) {
            fprintf(stderr, COLOR_GREEN "[SPY] open(\"%s\", %s", 
                    pathname, decode_flags(flags));
        } else {
            fprintf(stderr, COLOR_GREEN "[SPY] open(NULL, %s", decode_flags(flags));
        }
        if (flags & O_CREAT) {
            fprintf(stderr, ", 0%o", mode);
        }
        fprintf(stderr, ") = %d" COLOR_RESET "\n", result);
    } else {
        if (pathname) {
            fprintf(stderr, COLOR_RED "[SPY] open(\"%s\", %s) = -1 (errno=%d: %s)" COLOR_RESET "\n",
                    pathname, decode_flags(flags), errno, strerror(errno));
        } else {
            fprintf(stderr, COLOR_RED "[SPY] open(NULL, %s) = -1 (errno=%d: %s)" COLOR_RESET "\n",
                    decode_flags(flags), errno, strerror(errno));
        }
    }
    
    return result;
}

// =============== openat() interception ===============
int openat(int dirfd, const char *pathname, int flags, ...) __attribute__((nonnull (2)));
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
    
    openat_calls++;
    
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : 
                           (dirfd == -1) ? "-1" : "fd";
    
    if (result >= 0) {
        fprintf(stderr, COLOR_GREEN "[SPY] openat(%s", dirfd_str);
        if (dirfd >= 0 && dirfd != AT_FDCWD) fprintf(stderr, "=%d", dirfd);
        
        if (pathname) {
            fprintf(stderr, ", \"%s\", %s", pathname, decode_flags(flags));
        } else {
            fprintf(stderr, ", NULL, %s", decode_flags(flags));
        }
        
        if (flags & O_CREAT) {
            fprintf(stderr, ", 0%o", mode);
        }
        fprintf(stderr, ") = %d" COLOR_RESET "\n", result);
    } else {
        fprintf(stderr, COLOR_RED "[SPY] openat(%s", dirfd_str);
        if (dirfd >= 0 && dirfd != AT_FDCWD) fprintf(stderr, "=%d", dirfd);
        
        if (pathname) {
            fprintf(stderr, ", \"%s\", %s) = -1 (errno=%d: %s)" COLOR_RESET "\n",
                    pathname, decode_flags(flags), errno, strerror(errno));
        } else {
            fprintf(stderr, ", NULL, %s) = -1 (errno=%d: %s)" COLOR_RESET "\n",
                    decode_flags(flags), errno, strerror(errno));
        }
    }
    
    return result;
}

// =============== read() interception ===============
ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
    }
    
    ssize_t result = original_read(fd, buf, count);
    
    read_calls++;
    if (result > 0) total_bytes_read += result;
    
    // Only log non-stdin reads to reduce noise
    if (fd != 0) {
        if (result >= 0) {
            fprintf(stderr, COLOR_YELLOW "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd" COLOR_RESET "\n",
                    fd, buf, count, result);
        } else {
            fprintf(stderr, COLOR_RED "[SPY] read(fd=%d, buf=%p, count=%zu) = -1 (errno=%d: %s)" COLOR_RESET "\n",
                    fd, buf, count, errno, strerror(errno));
        }
    }
    
    return result;
}

// =============== write() interception ===============
ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
    }
    
    ssize_t result = original_write(fd, buf, count);
    
    write_calls++;
    if (result > 0) total_bytes_written += result;
    
    // Don't log stderr writes to avoid recursion
    if (fd != 2) {
        if (result >= 0) {
            // For stdout, show a snippet of what's being written (if small)
            if (fd == 1 && count <= 80 && result > 0 && buf) {
                fprintf(stderr, COLOR_YELLOW "[SPY] write(fd=%d, \"%.80s\"%s, count=%zu) = %zd" COLOR_RESET "\n",
                        fd, (char*)buf, (count > 80) ? "..." : "", count, result);
            } else {
                fprintf(stderr, COLOR_YELLOW "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd" COLOR_RESET "\n",
                        fd, buf, count, result);
            }
        } else {
            fprintf(stderr, COLOR_RED "[SPY] write(fd=%d, buf=%p, count=%zu) = -1 (errno=%d: %s)" COLOR_RESET "\n",
                    fd, buf, count, errno, strerror(errno));
        }
    }
    
    return result;
}

// =============== close() interception ===============
int close(int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = dlsym(RTLD_NEXT, "close");
    }
    
    int result = original_close(fd);
    
    close_calls++;
    
    if (result >= 0) {
        fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, result);
    } else {
        fprintf(stderr, COLOR_RED "[SPY] close(fd=%d) = -1 (errno=%d: %s)" COLOR_RESET "\n",
                fd, errno, strerror(errno));
    }
    
    return result;
}
