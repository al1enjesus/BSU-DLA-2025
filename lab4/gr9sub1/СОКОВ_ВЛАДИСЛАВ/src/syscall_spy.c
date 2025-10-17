#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h> // Для opendir, readdir, closedir

// Безопасный вывод в stderr: используем write(2) оригинальный через dlsym(RTLD_NEXT, "write")
static ssize_t (*real_write)(int, const void*, size_t) = NULL;
static void ensure_real_write(void) {
    if (!real_write) real_write = dlsym(RTLD_NEXT, "write");
}

static void spy_fprintf(const char *s) {
    ensure_real_write();
    if (!real_write) return; // если не удалось получить, молча выходим
    size_t len = 0;
    while (s[len]) ++len;
    // Пишем сразу в stderr (fd=2) чтобы не использовать fprintf (рекурсия)
    real_write(2, s, len);
}

// ===== open =====
int open(const char *pathname, int flags, ...) {
    static int (*orig_open)(const char*, int, ...) = NULL;
    if (!orig_open) orig_open = dlsym(RTLD_NEXT, "open");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap; va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap);
        int res = orig_open(pathname, flags, mode);
        char buf[512];
        int n = snprintf(buf, sizeof(buf), "[SPY] open(\"%s\", flags=0x%x, mode=0%o) = %d\n", pathname, flags, mode, res);
        if (n>0) spy_fprintf(buf);
        return res;
    } else {
        int res = orig_open(pathname, flags);
        char buf[512];
        int n = snprintf(buf, sizeof(buf), "[SPY] open(\"%s\", flags=0x%x) = %d\n", pathname, flags, res);
        if (n>0) spy_fprintf(buf);
        return res;
    }
}

// ===== openat =====
int openat(int dirfd, const char *pathname, int flags, ...) {
    static int (*orig_openat)(int, const char*, int, ...) = NULL;
    if (!orig_openat) orig_openat = dlsym(RTLD_NEXT, "openat");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap; va_start(ap, flags); mode = va_arg(ap, mode_t); va_end(ap);
        int res = orig_openat(dirfd, pathname, flags, mode);
        char buf[512];
        const char *dirfd_s = (dirfd==AT_FDCWD) ? "AT_FDCWD" : "<fd>";
        int n = snprintf(buf, sizeof(buf), "[SPY] openat(%s, \"%s\", flags=0x%x, mode=0%o) = %d\n", dirfd_s, pathname, flags, mode, res);
        if (n>0) spy_fprintf(buf);
        return res;
    } else {
        int res = orig_openat(dirfd, pathname, flags);
        char buf[512];
        const char *dirfd_s = (dirfd==AT_FDCWD) ? "AT_FDCWD" : "<fd>";
        int n = snprintf(buf, sizeof(buf), "[SPY] openat(%s, \"%s\", flags=0x%x) = %d\n", dirfd_s, pathname, flags, res);
        if (n>0) spy_fprintf(buf);
        return res;
    }
}

// ===== read =====
ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*orig_read)(int, void*, size_t) = NULL;
    if (!orig_read) orig_read = dlsym(RTLD_NEXT, "read");
    ssize_t res = orig_read(fd, buf, count);
    char out[256];
    int n = snprintf(out, sizeof(out), "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, res);
    if (n>0) spy_fprintf(out);
    return res;
}

// ===== write =====
ssize_t write(int fd, const void *buf, size_t count) {
    // Получаем оригинал только один раз
    if (!real_write) real_write = dlsym(RTLD_NEXT, "write");
    // Важно: не логируем сами записи в stderr (fd==2) иначе рекурсия
    if (fd == 2) {
        return real_write ? real_write(fd, buf, count) : -1;
    }
    ssize_t res = real_write ? real_write(fd, buf, count) : -1;
    char out[256];
    int n = snprintf(out, sizeof(out), "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd\n", fd, buf, count, res);
    if (n>0) spy_fprintf(out);
    return res;
}

// ===== opendir =====
DIR *opendir(const char *name) {
    static DIR *(*orig_opendir)(const char *) = NULL;
    if (!orig_opendir) orig_opendir = dlsym(RTLD_NEXT, "opendir");

    DIR *res = orig_opendir(name);
    char buf[512];
    int n = snprintf(buf, sizeof(buf), "[SPY] opendir(\"%s\") = %p\n", name, res);
    if (n > 0) spy_fprintf(buf);
    return res;
}

// ===== readdir =====
struct dirent *readdir(DIR *dirp) {
    static struct dirent *(*orig_readdir)(DIR *) = NULL;
    if (!orig_readdir) orig_readdir = dlsym(RTLD_NEXT, "readdir");

    struct dirent *res = orig_readdir(dirp);
    char buf[512];
    // Логируем имя файла, если он не NULL
    const char *d_name = res ? res->d_name : "NULL";
    int n = snprintf(buf, sizeof(buf), "[SPY] readdir(%p) -> d_name: %s\n", dirp, d_name);
    if (n > 0) spy_fprintf(buf);
    return res;
}

// ===== closedir =====
int closedir(DIR *dirp) {
    static int (*orig_closedir)(DIR *) = NULL;
    if (!orig_closedir) orig_closedir = dlsym(RTLD_NEXT, "closedir");

    int res = orig_closedir(dirp);
    char buf[256];
    int n = snprintf(buf, sizeof(buf), "[SPY] closedir(%p) = %d\n", dirp, res);
    if (n > 0) spy_fprintf(buf);
    return res;
}

// ===== close =====
int close(int fd) {
    static int (*orig_close)(int) = NULL;
    if (!orig_close) orig_close = dlsym(RTLD_NEXT, "close");
    int res = orig_close(fd);
    char out[128];
    int n = snprintf(out, sizeof(out), "[SPY] close(fd=%d) = %d\n", fd, res);
    if (n>0) spy_fprintf(out);
    return res;
}