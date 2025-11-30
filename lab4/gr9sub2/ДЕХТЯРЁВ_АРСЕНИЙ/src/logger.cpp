#define _GNU_SOURCE
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>

static FILE* logf = nullptr;

static void init_log() {
    if (!logf) {
        logf = fopen("/tmp/syscall_log.txt", "a");
        if (!logf) logf = stderr;
    }
}

extern "C" int open(const char* path, int flags, ...) {
    static int (*real_open)(const char*, int, mode_t) = nullptr;
    if (!real_open)
        real_open = (int (*)(const char*, int, mode_t)) dlsym(RTLD_NEXT, "open");

    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    init_log();
    fprintf(logf, "[open] path=%s flags=%d mode=%o\n", path, flags, mode);
    fflush(logf);

    return real_open(path, flags, mode);
}

extern "C" int execve(const char *filename, char *const argv[], char *const envp[]) {
    static int (*real_execve)(const char*, char* const[], char* const[]) = nullptr;
    if (!real_execve)
        real_execve = (int (*)(const char*, char* const[], char* const[]))
            dlsym(RTLD_NEXT, "execve");

    init_log();
    fprintf(logf, "[execve] %s\n", filename);
    fflush(logf);

    return real_execve(filename, argv, envp);
}

extern "C" ssize_t write(int fd, const void* buf, size_t count) {
    static ssize_t (*real_write)(int, const void*, size_t) = nullptr;
    if (!real_write)
        real_write = (ssize_t (*)(int, const void*, size_t))
            dlsym(RTLD_NEXT, "write");

    init_log();
    fprintf(logf, "[write] fd=%d count=%zu\n", fd, count);
    fflush(logf);

    return real_write(fd, buf, count);
}
