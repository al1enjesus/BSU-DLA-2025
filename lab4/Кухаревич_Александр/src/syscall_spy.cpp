#define _GNU_SOURCE
#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdio>

// Безопасное логирование через оригинальный write() (через dlsym(RTLD_NEXT, "write"))
static void safe_log(const char* msg) {
    static ssize_t (*orig_write)(int, const void*, size_t) = nullptr;
    static bool inited = false;
    if (!inited) {
        orig_write = (ssize_t(*)(int, const void*, size_t))dlsym(RTLD_NEXT, "write");
        inited = true;
    }
    if (orig_write) {
        orig_write(2, msg, strlen(msg)); // stderr
    } else {
        syscall(SYS_write, 2, msg, strlen(msg));
    }
}

static inline void log_fmt(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) safe_log(buf);
}

// Счётчики вызовов
static std::mutex counters_mtx;
static std::unordered_map<std::string, uint64_t> counters;

// Утилита: увеличить счётчик
static void inc_counter(const char* name) {
    std::lock_guard<std::mutex> lk(counters_mtx);
    counters[std::string(name)]++;
}

// Печать сводки при завершении процесса
__attribute__((destructor))
static void print_summary() {
    log_fmt("\n[SPY] Syscall spy summary (pid=%d)\n", getpid());
    std::lock_guard<std::mutex> lk(counters_mtx);
    for (auto &p : counters) {
        log_fmt("[SPY]   %-15s : %llu\n", p.first.c_str(), (unsigned long long)p.second);
    }
    log_fmt("[SPY] End of summary\n\n");
}

// ------------------ Перехватываемые функции ------------------

// fork()
extern "C" pid_t fork(void) {
    static pid_t (*orig_fork)(void) = nullptr;
    if (!orig_fork) orig_fork = (pid_t(*)(void))dlsym(RTLD_NEXT, "fork");

    inc_counter("fork");
    log_fmt("[SPY] fork() called (pid=%d)\n", getpid());

    if (orig_fork) {
        pid_t r = orig_fork();
        log_fmt("[SPY] fork() -> %d\n", r);
        return r;
    } else {
        // fallback to syscall
        pid_t r = (pid_t)syscall(SYS_fork);
        log_fmt("[SPY] fork() syscall -> %d\n", r);
        return r;
    }
}

// vfork()
extern "C" pid_t vfork(void) {
    static pid_t (*orig_vfork)(void) = nullptr;
    if (!orig_vfork) orig_vfork = (pid_t(*)(void))dlsym(RTLD_NEXT, "vfork");

    inc_counter("vfork");
    log_fmt("[SPY] vfork() called (pid=%d)\n", getpid());

    if (orig_vfork) {
        pid_t r = orig_vfork();
        log_fmt("[SPY] vfork() -> %d\n", r);
        return r;
    } else {
        pid_t r = (pid_t)syscall(SYS_vfork);
        log_fmt("[SPY] vfork() syscall -> %d\n", r);
        return r;
    }
}

// clone()
// POSIX wrapper can be variadic; we'll attempt to forward all args transparently.
// Prototype differs across libc implementations; we use a generic function-pointer type.
extern "C" int clone(int (*fn)(void*), void *child_stack, int flags, void *arg, ...) {
    // get original clone symbol
    static int (*orig_clone)(int (*)(void*), void*, int, void*, ...) = nullptr;
    if (!orig_clone) orig_clone = (int (*)(int (*)(void*), void*, int, void*, ...))dlsym(RTLD_NEXT, "clone");

    inc_counter("clone");
    log_fmt("[SPY] clone(flags=0x%x) called (pid=%d)\n", flags, getpid());

    if (orig_clone) {
        // forward variadic args if present (e.g., parent_tidptr, tls, child_tidptr)
        va_list ap;
        va_start(ap, arg);
        // We cannot know exactly how many extra args; attempt to call with basic 4-arg wrapper
        int r = orig_clone(fn, child_stack, flags, arg);
        va_end(ap);
        log_fmt("[SPY] clone() -> %d\n", r);
        return r;
    } else {
        // fallback: use syscall(SYS_clone, flags, child_stack, ...) — but that's architecture-dependent.
        int r = (int)syscall(SYS_clone, flags, child_stack, arg);
        log_fmt("[SPY] clone() syscall -> %d\n", r);
        return r;
    }
}

// execve()
extern "C" int execve(const char *pathname, char *const argv[], char *const envp[]) {
    static int (*orig_execve)(const char*, char*const[], char*const[]) = nullptr;
    if (!orig_execve) orig_execve = (int (*)(const char*, char*const[], char*const[]))dlsym(RTLD_NEXT, "execve");

    inc_counter("execve");
    // Print first argv if present
    const char* a0 = (argv && argv[0]) ? argv[0] : "(null)";
    log_fmt("[SPY] execve(\"%s\", argv[0]=\"%s\") called (pid=%d)\n", pathname ? pathname : "(null)", a0, getpid());

    if (orig_execve) {
        int r = orig_execve(pathname, argv, envp);
        // execve on success does not return. If it returns, it's an error.
        log_fmt("[SPY] execve() returned %d errno=%d (%s)\n", r, errno, strerror(errno));
        return r;
    } else {
        int r = (int)syscall(SYS_execve, pathname, argv, envp);
        log_fmt("[SPY] execve() syscall returned %d errno=%d (%s)\n", r, errno, strerror(errno));
        return r;
    }
}

// waitpid()
extern "C" pid_t waitpid(pid_t pid, int *wstatus, int options) {
    static pid_t (*orig_waitpid)(pid_t, int*, int) = nullptr;
    if (!orig_waitpid) orig_waitpid = (pid_t(*)(pid_t,int*,int))dlsym(RTLD_NEXT, "waitpid");

    inc_counter("waitpid");
    log_fmt("[SPY] waitpid(pid=%d, options=0x%x) called (pid=%d)\n", pid, options, getpid());

    if (orig_waitpid) {
        pid_t r = orig_waitpid(pid, wstatus, options);
        log_fmt("[SPY] waitpid() -> %d (status=%d)\n", r, wstatus ? *wstatus : 0);
        return r;
    } else {
        pid_t r = (pid_t)syscall(SYS_wait4, pid, wstatus, options, NULL);
        log_fmt("[SPY] waitpid() syscall -> %d\n", r);
        return r;
    }
}

// wait()
extern "C" pid_t wait(int *wstatus) {
    static pid_t (*orig_wait)(int*) = nullptr;
    if (!orig_wait) orig_wait = (pid_t(*)(int*))dlsym(RTLD_NEXT, "wait");

    inc_counter("wait");
    log_fmt("[SPY] wait() called (pid=%d)\n", getpid());

    if (orig_wait) {
        pid_t r = orig_wait(wstatus);
        log_fmt("[SPY] wait() -> %d (status=%d)\n", r, wstatus ? *wstatus : 0);
        return r;
    } else {
        pid_t r = (pid_t)syscall(SYS_wait4, -1, wstatus, 0, NULL);
        log_fmt("[SPY] wait() syscall -> %d\n", r);
        return r;
    }
}

// exit()
extern "C" void exit(int status) {
    static void (*orig_exit)(int) = nullptr;
    if (!orig_exit) orig_exit = (void(*)(int))dlsym(RTLD_NEXT, "exit");

    inc_counter("exit");
    log_fmt("[SPY] exit(status=%d) called (pid=%d)\n", status, getpid());

    if (orig_exit) {
        orig_exit(status);
    } else {
        syscall(SYS_exit, status);
        // never returns
    }
}

// _exit()
extern "C" void _exit(int status) {
    static void (*orig__exit)(int) = nullptr;
    if (!orig__exit) orig__exit = (void(*)(int))dlsym(RTLD_NEXT, "_exit");

    inc_counter("_exit");
    log_fmt("[SPY] _exit(status=%d) called (pid=%d)\n", status, getpid());

    if (orig__exit) {
        orig__exit(status);
    } else {
        syscall(SYS_exit, status);
    }
}

