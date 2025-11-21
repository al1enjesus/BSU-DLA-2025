#ifndef SYSCALL_NAMES_H
#define SYSCALL_NAMES_H

#include <string.h>

// Массив имён системных вызовов для x86_64
// Индекс = номер syscall, значение = имя
static const char *syscall_names[] = {
    [0] = "read",
    [1] = "write",
    [2] = "open",
    [3] = "close",
    [4] = "stat",
    [5] = "fstat",
    [6] = "lstat",
    [7] = "poll",
    [8] = "lseek",
    [9] = "mmap",
    [10] = "mprotect",
    [11] = "munmap",
    [12] = "brk",
    [13] = "rt_sigaction",
    [14] = "rt_sigprocmask",
    [15] = "rt_sigreturn",
    [21] = "access",
    [22] = "pipe",
    [25] = "mremap",
    [32] = "dup",
    [33] = "dup2",
    [35] = "nanosleep",
    [39] = "getpid",
    [56] = "clone",
    [57] = "fork",
    [58] = "vfork",
    [59] = "execve",
    [60] = "exit",
    [61] = "wait4",
    [62] = "kill",
    [63] = "uname",
    [72] = "fcntl",
    [78] = "getdents",
    [79] = "getcwd",
    [80] = "chdir",
    [89] = "readlink",
    [96] = "gettimeofday",
    [97] = "getrlimit",
    [102] = "getuid",
    [104] = "getgid",
    [107] = "geteuid",
    [108] = "getegid",
    [110] = "getppid",
    [111] = "getpgrp",
    [127] = "rt_sigpending",
    [131] = "sigaltstack",
    [158] = "arch_prctl",
    [201] = "time",
    [217] = "getdents64",
    [228] = "clock_gettime",
    [231] = "exit_group",
    [257] = "openat",
    [262] = "newfstatat",
    [270] = "pselect6",
    [288] = "prlimit64",
    [302] = "prctl",
    [318] = "getrandom",
};

const char *get_syscall_name(long long syscall_num) {
    if (syscall_num >= 0 && syscall_num < sizeof(syscall_names) / sizeof(syscall_names[0])) {
        const char *name = syscall_names[syscall_num];
        if (name) {
            return name;
        }
    }
    return "unknown";
}

#endif // SYSCALL_NAMES_H
