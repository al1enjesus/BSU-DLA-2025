/**
 * syscall_spy.c - LD_PRELOAD library for intercepting system calls
 * Lab 4, Variant 25 (СТОЯН_ЮРИЙ)
 *
 * Intercepts: open(), openat(), read(), write(), close()
  * Programs: find, tar, cp
  */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

  // Color codes
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RESET   "\033[0m"

// Statistics
static struct {
    long open, openat, read, write, close;
    long bytes_read, bytes_written;
} stats = { 0 };

static char current_program[64] = "unknown";

// Constructor
__attribute__((constructor))
void init() {
    FILE* cmdline = fopen("/proc/self/cmdline", "r");
    if (cmdline) {
        fread(current_program, 1, sizeof(current_program) - 1, cmdline);
        current_program[strcspn(current_program, "\n\r\t ")] = '\0';
        fclose(cmdline);
    }

    fprintf(stderr, COLOR_CYAN "\n[SPY] === Variant: find/tar/cp === Program: %s ===\n" COLOR_RESET,
        current_program);
}

// Destructor
__attribute__((destructor))
void fini() {
    fprintf(stderr, COLOR_CYAN "\n[SPY] === Statistics for %s ===\n", current_program);
    fprintf(stderr, "[SPY] Open: %ld, Openat: %ld\n", stats.open, stats.openat);
    fprintf(stderr, "[SPY] Read: %ld (%ld bytes), Write: %ld (%ld bytes)\n",
        stats.read, stats.bytes_read, stats.write, stats.bytes_written);
    fprintf(stderr, "[SPY] Close: %ld\n", stats.close);
    fprintf(stderr, "[SPY] ===============================\n" COLOR_RESET);
}

// Helper to check if we should log this file (filter out system files)
int should_log(const char* filename) {
    if (!filename) return 0;
    const char* ignore[] = {
        "/proc/", "/sys/", "/dev/", "/etc/ld.so", ".so",
        "/lib/", "/usr/lib/", NULL
    };

    for (int i = 0; ignore[i]; i++) {
        if (strstr(filename, ignore[i])) {
            return 0;
        }
    }

    return 1;
}

// ========== open() interception ==========
int open(const char* pathname, int flags, ...) {
    static int (*real_open)(const char*, int, ...) = NULL;
    if (!real_open) real_open = dlsym(RTLD_NEXT, "open");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = real_open(pathname, flags, mode);
    stats.open++;

    if (should_log(pathname)) {
        if (result >= 0) {
            fprintf(stderr, COLOR_GREEN "[SPY] OPEN: \"%s\" -> fd=%d\n" COLOR_RESET, pathname, result);
        }
        else {
            fprintf(stderr, COLOR_RED "[SPY] OPEN FAIL: \"%s\" (%s)\n" COLOR_RESET,
                pathname, strerror(errno));
        }
    }

    return result;
}

// ========== openat() interception ==========
int openat(int dirfd, const char* pathname, int flags, ...) {
    static int (*real_openat)(int, const char*, int, ...) = NULL;
    if (!real_openat) real_openat = dlsym(RTLD_NEXT, "openat");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int result = real_openat(dirfd, pathname, flags, mode);
    stats.openat++;

    if (should_log(pathname)) {
        const char* dir_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "dirfd";

        if (result >= 0) {
            fprintf(stderr, COLOR_GREEN "[SPY] OPENAT: %s/\"%s\" -> fd=%d\n" COLOR_RESET,
                dir_str, pathname, result);
        }
        else {
            fprintf(stderr, COLOR_RED "[SPY] OPENAT FAIL: %s/\"%s\" (%s)\n" COLOR_RESET,
                dir_str, pathname, strerror(errno));
        }
    }

    return result;
}

// ========== read() interception ==========
ssize_t read(int fd, void* buf, size_t count) {
    static ssize_t(*real_read)(int, void*, size_t) = NULL;
    if (!real_read) real_read = dlsym(RTLD_NEXT, "read");

    ssize_t result = real_read(fd, buf, count);
    stats.read++;
    if (result > 0) stats.bytes_read += result;
    if (fd > 2 && count > 0 && result > 0) {
        fprintf(stderr, COLOR_YELLOW "[SPY] READ: fd=%d, bytes=%zd\n" COLOR_RESET, fd, result);
    }

    return result;
}

// ========== write() interception ==========
ssize_t write(int fd, const void* buf, size_t count) {
    static ssize_t(*real_write)(int, const void*, size_t) = NULL;
    if (!real_write) real_write = dlsym(RTLD_NEXT, "write");

    ssize_t result = real_write(fd, buf, count);
    stats.write++;
    if (result > 0) stats.bytes_written += result;
    if (fd != 2 && count > 0) {
        fprintf(stderr, COLOR_MAGENTA "[SPY] WRITE: fd=%d, bytes=%zd\n" COLOR_RESET, fd, result);
    }

    return result;
}

// ========== close() interception ==========
int close(int fd) {
    static int (*real_close)(int) = NULL;
    if (!real_close) real_close = dlsym(RTLD_NEXT, "close");

    int result = real_close(fd);
    stats.close++;

    fprintf(stderr, "[SPY] CLOSE: fd=%d\n", fd);

    return result;
}