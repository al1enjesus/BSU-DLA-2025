#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static void print_open_flags(int flags) {
    struct FlagName {
        int flag;
        const char* name;
    } typedef FlagName;

    const FlagName access_flags[3] = {
        {O_RDONLY, "O_RDONLY"},
        {O_WRONLY, "O_WRONLY"},
        {O_RDWR, "O_RDWR"},
    };

    const FlagName additional_flags[6] = {
        {O_CREAT, "O_CREAT"},
        {O_TRUNC, "O_TRUNC"},
        {O_APPEND, "O_APPEND"},
        {O_EXCL, "O_EXCL"},
        {O_NONBLOCK, "O_NONBLOCK"},
        {O_CLOEXEC, "O_CLOEXEC"},
    };

    fprintf(stderr, "flags=");

    const int mode = flags & O_ACCMODE;
    for (int i = 0; i < 3; i++) {
        if (mode ==  access_flags[i].flag) {
            fprintf(stderr,  "%s", access_flags[i].name);
            break;
        }
    }

    for (int i = 0; i < 6; i++) {
        if (flags & additional_flags[i].flag)
            fprintf(stderr,  "%s|", additional_flags[i].name);
    }

    fprintf(stderr, " (0x%x)", flags);
}

int open(const char *pathname, int flags, ...) {
    static int (*original_open)(const char*, int, ...) = NULL;
    if (!original_open) {
        original_open = dlsym(RTLD_NEXT, "open");
        if (!original_open) {
            fprintf(stderr, "[SPY] ERROR: Cannot find original open()\n");
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

    const int result = original_open(pathname, flags, mode);
    fprintf(stderr, "[SPY] open(\"%s\", ", pathname);
    print_open_flags(flags);
    if (flags & O_CREAT) {
        fprintf(stderr, ", mode=%o", mode);
    }
    fprintf(stderr, ") = %d", result);
    if (result == -1) {
        fprintf(stderr, " [errno=%d: %s]", errno, strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}


int openat(int dirfd, const char *pathname, int flags, ...) {
    static int (*original_openat)(int, const char*, int, ...) = NULL;
    if (!original_openat) {
        original_openat = dlsym(RTLD_NEXT, "openat");
        if (!original_openat) {
            fprintf(stderr, "[SPY] ERROR: Cannot find original openat()\n");
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

    fprintf(stderr, "[SPY] openat(");
    if (dirfd == AT_FDCWD) {
        fprintf(stderr, "AT_FDCWD");
    } else {
        fprintf(stderr, "fd=%d", dirfd);
    }
    fprintf(stderr, ", \"%s\", ", pathname);
    print_open_flags(flags);
    if (flags & O_CREAT) {
        fprintf(stderr, ", mode=%o", mode);
    }
    fprintf(stderr, ") = %d", result);
    if (result == -1) {
        fprintf(stderr, " [errno=%d: %s]", errno, strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}

int open64(const char *pathname, int flags, ...) {
    va_list args;
    va_start(args, flags);
    mode_t mode = 0;
    if (flags & O_CREAT) {
        mode = va_arg(args, mode_t);
    }
    va_end(args);

    return open(pathname, flags, mode);
}

int openat64(int dirfd, const char *pathname, int flags, ...) {
    va_list args;
    va_start(args, flags);
    mode_t mode = 0;
    if (flags & O_CREAT) {
        mode = va_arg(args, mode_t);
    }
    va_end(args);

    return openat(dirfd, pathname, flags, mode);
}

ssize_t read(int fd, void *buf, size_t count) {
    static ssize_t (*original_read)(int, void*, size_t) = NULL;
    if (!original_read) {
        original_read = dlsym(RTLD_NEXT, "read");
        if (!original_read) {
            fprintf(stderr, "[SPY] ERROR: Cannot find original read()\n");
            return -1;
        }
    }

    ssize_t result = original_read(fd, buf, count);

    fprintf(stderr, "[SPY] read(fd=%d, buf=%p, count=%zu) = %zd",
            fd, buf, count, result);
    if (result == -1) {
        fprintf(stderr, " [errno=%d: %s]", errno, strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}

ssize_t write(int fd, const void *buf, size_t count) {
    static ssize_t (*original_write)(int, const void*, size_t) = NULL;
    if (!original_write) {
        original_write = dlsym(RTLD_NEXT, "write");
        if (!original_write) {
            // Не можем использовать fprintf, так как он использует write
            return -1;
        }
    }

    ssize_t result = original_write(fd, buf, count);

    if (fd != 2) {
        fprintf(stderr, "[SPY] write(fd=%d, buf=%p, count=%zu) = %zd",
                fd, buf, count, result);
        if (result == -1) {
            fprintf(stderr, " [errno=%d: %s]", errno, strerror(errno));
        }

        // Опционально: показываем содержимое для stdout и небольших буферов
        if (fd == 1 && count < 100 && result > 0) {
            fprintf(stderr, " [data: \"");
            for (ssize_t i = 0; i < result && i < 50; i++) {
                char c = ((char*)buf)[i];
                if (c >= 32 && c <= 126) {
                    fprintf(stderr, "%c", c);
                } else if (c == '\n') {
                    fprintf(stderr, "\\n");
                } else {
                    fprintf(stderr, ".");
                }
            }
            if (result > 50) fprintf(stderr, "...");
            fprintf(stderr, "\"]");
        }

        fprintf(stderr, "\n");
    }

    return result;
}

int close(const int fd) {
    static int (*original_close)(int) = NULL;
    if (!original_close) {
        original_close = (int(*)(int))dlsym(RTLD_NEXT, "close");
        if (!original_close) {
            fprintf(stderr, "[SPY] ERROR: Cannot find original close()\n");
            return -1;
        }
    }

    int result = original_close(fd);

    fprintf(stderr, "[SPY] close(fd=%d) = %d", fd, result);
    if (result == -1) {
        fprintf(stderr, " [errno=%d: %s]", errno, strerror(errno));
    }
    fprintf(stderr, "\n");

    return result;
}

__attribute__((constructor))
static void init_spy() {
    fprintf(stderr, "\n[SPY] ═══════════════════════════════════════════════\n");
    fprintf(stderr, "[SPY] Syscall spy library loaded (LD_PRELOAD)\n");
    fprintf(stderr, "[SPY] Intercepting: open, openat, read, write, close\n");
    fprintf(stderr, "[SPY] ═══════════════════════════════════════════════\n\n");
}

__attribute__((destructor))
static void fini_spy() {
    fprintf(stderr, "\n[SPY] ═══════════════════════════════════════════════\n");
    fprintf(stderr, "[SPY] Syscall spy library unloaded\n");
    fprintf(stderr, "[SPY] ═══════════════════════════════════════════════\n\n");
}