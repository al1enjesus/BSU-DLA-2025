#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>

// Функция инициализации - вызывается при загрузке библиотеки
__attribute__((constructor)) void init_library() {
    fprintf(stderr, "[SPY] Library loaded successfully! (WSL version)\n");
}

// Упрощенный вывод флагов
const char* get_flags_str(int flags) {
    if (flags == O_RDONLY) return "O_RDONLY";
    if (flags == O_WRONLY) return "O_WRONLY"; 
    if (flags == O_RDWR) return "O_RDWR";
    return "UNKNOWN";
}

// ========== ПЕРЕХВАТ ФАЙЛОВЫХ ОПЕРАЦИЙ ==========

// open - устаревший, но для совместимости
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
    
    int result = orig_open(pathname, flags, mode);
    fprintf(stderr, "[SPY] open('%s', %s) = %d\n", pathname, get_flags_str(flags & O_ACCMODE), result);
    return result;
}

// openat - основной системный вызов в современных системах
int openat(int dirfd, const char *pathname, int flags, ...) {
    static int (*orig_openat)(int, const char*, int, ...) = NULL;
    if (!orig_openat) orig_openat = dlsym(RTLD_NEXT, "openat");
    
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    int result = orig_openat(dirfd, pathname, flags, mode);
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "dirfd";
    fprintf(stderr, "[SPY] openat(%s, '%s', %s) = %d\n", dirfd_str, pathname, get_flags_str(flags & O_ACCMODE), result);
    return result;
}

// open64 - 64-битная версия
int open64(const char *pathname, int flags, ...) {
    static int (*orig_open64)(const char*, int, ...) = NULL;
    if (!orig_open64) orig_open64 = dlsym(RTLD_NEXT, "open64");
    
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    int result = orig_open64(pathname, flags, mode);
    fprintf(stderr, "[SPY] open64('%s', %s) = %d\n", pathname, get_flags_str(flags & O_ACCMODE), result);
    return result;
}

// openat64 - 64-битная версия openat
int openat64(int dirfd, const char *pathname, int flags, ...) {
    static int (*orig_openat64)(int, const char*, int, ...) = NULL;
    if (!orig_openat64) orig_openat64 = dlsym(RTLD_NEXT, "openat64");
    
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    
    int result = orig_openat64(dirfd, pathname, flags, mode);
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "dirfd";
    fprintf(stderr, "[SPY] openat64(%s, '%s', %s) = %d\n", dirfd_str, pathname, get_flags_str(flags & O_ACCMODE), result);
    return result;
}

// ========== ПЕРЕХВАТ ЧТЕНИЯ/ЗАПИСИ ==========

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*orig_read)(int, void*, size_t) = NULL;
    if (!orig_read) orig_read = dlsym(RTLD_NEXT, "read");
    
    ssize_t result = orig_read(fd, buf, count);
    fprintf(stderr, "[SPY] read(fd=%d, count=%zu) = %zd\n", fd, count, result);
    return result;
}

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*orig_write)(int, const void*, size_t) = NULL;
    if (!orig_write) orig_write = dlsym(RTLD_NEXT, "write");
    
    // Избегаем рекурсии для stderr
    ssize_t result = orig_write(fd, buf, count);
    if (fd != 2) { // не stderr
        fprintf(stderr, "[SPY] write(fd=%d, count=%zu) = %zd\n", fd, count, result);
    }
    return result;
}

// ========== ПЕРЕХВАТ ЗАКРЫТИЯ ==========

int close(int fd) {
    static int (*orig_close)(int) = NULL;
    if (!orig_close) orig_close = dlsym(RTLD_NEXT, "close");
    
    int result = orig_close(fd);
    fprintf(stderr, "[SPY] close(fd=%d) = %d\n", fd, result);
    return result;
}

// ========== ПЕРЕХВАТ ФУНКЦИЙ СТАТ ==========

int stat(const char *pathname, struct stat *statbuf) {
    static int (*orig_stat)(const char*, struct stat*) = NULL;
    if (!orig_stat) orig_stat = dlsym(RTLD_NEXT, "stat");
    
    int result = orig_stat(pathname, statbuf);
    fprintf(stderr, "[SPY] stat('%s') = %d\n", pathname, result);
    return result;
}

int lstat(const char *pathname, struct stat *statbuf) {
    static int (*orig_lstat)(const char*, struct stat*) = NULL;
    if (!orig_lstat) orig_lstat = dlsym(RTLD_NEXT, "lstat");
    
    int result = orig_lstat(pathname, statbuf);
    fprintf(stderr, "[SPY] lstat('%s') = %d\n", pathname, result);
    return result;
}

int fstat(int fd, struct stat *statbuf) {
    static int (*orig_fstat)(int, struct stat*) = NULL;
    if (!orig_fstat) orig_fstat = dlsym(RTLD_NEXT, "fstat");
    
    int result = orig_fstat(fd, statbuf);
    fprintf(stderr, "[SPY] fstat(fd=%d) = %d\n", fd, result);
    return result;
}

// 64-битные версии
int stat64(const char *pathname, struct stat64 *statbuf) {
    static int (*orig_stat64)(const char*, struct stat64*) = NULL;
    if (!orig_stat64) orig_stat64 = dlsym(RTLD_NEXT, "stat64");
    
    int result = orig_stat64(pathname, statbuf);
    fprintf(stderr, "[SPY] stat64('%s') = %d\n", pathname, result);
    return result;
}

int lstat64(const char *pathname, struct stat64 *statbuf) {
    static int (*orig_lstat64)(const char*, struct stat64*) = NULL;
    if (!orig_lstat64) orig_lstat64 = dlsym(RTLD_NEXT, "lstat64");
    
    int result = orig_lstat64(pathname, statbuf);
    fprintf(stderr, "[SPY] lstat64('%s') = %d\n", pathname, result);
    return result;
}

int fstat64(int fd, struct stat64 *statbuf) {
    static int (*orig_fstat64)(int, struct stat64*) = NULL;
    if (!orig_fstat64) orig_fstat64 = dlsym(RTLD_NEXT, "fstat64");
    
    int result = orig_fstat64(fd, statbuf);
    fprintf(stderr, "[SPY] fstat64(fd=%d) = %d\n", fd, result);
    return result;
}

// ========== ПЕРЕХВАТ ДИРЕКТОРИЙ ==========

DIR *opendir(const char *name) {
    static DIR *(*orig_opendir)(const char*) = NULL;
    if (!orig_opendir) orig_opendir = dlsym(RTLD_NEXT, "opendir");
    
    DIR *result = orig_opendir(name);
    fprintf(stderr, "[SPY] opendir('%s') = %p\n", name, result);
    return result;
}

struct dirent *readdir(DIR *dirp) {
    static struct dirent *(*orig_readdir)(DIR*) = NULL;
    if (!orig_readdir) orig_readdir = dlsym(RTLD_NEXT, "readdir");
    
    struct dirent *result = orig_readdir(dirp);
    if (result) {
        fprintf(stderr, "[SPY] readdir(%p) = '%s'\n", dirp, result->d_name);
    }
    return result;
}

int closedir(DIR *dirp) {
    static int (*orig_closedir)(DIR*) = NULL;
    if (!orig_closedir) orig_closedir = dlsym(RTLD_NEXT, "closedir");
    
    int result = orig_closedir(dirp);
    fprintf(stderr, "[SPY] closedir(%p) = %d\n", dirp, result);
    return result;
}

// ========== ПЕРЕХВАТ ACCESS ==========

int access(const char *pathname, int mode) {
    static int (*orig_access)(const char*, int) = NULL;
    if (!orig_access) orig_access = dlsym(RTLD_NEXT, "access");
    
    int result = orig_access(pathname, mode);
    fprintf(stderr, "[SPY] access('%s', 0x%x) = %d\n", pathname, mode, result);
    return result;
}

int faccessat(int dirfd, const char *pathname, int mode, int flags) {
    static int (*orig_faccessat)(int, const char*, int, int) = NULL;
    if (!orig_faccessat) orig_faccessat = dlsym(RTLD_NEXT, "faccessat");
    
    int result = orig_faccessat(dirfd, pathname, mode, flags);
    const char *dirfd_str = (dirfd == AT_FDCWD) ? "AT_FDCWD" : "dirfd";
    fprintf(stderr, "[SPY] faccessat(%s, '%s', 0x%x, 0x%x) = %d\n", 
            dirfd_str, pathname, mode, flags, result);
    return result;
}