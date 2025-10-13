#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/reg.h>

#include "syscall_names.h"

// Функция для чтения строки из памяти дочернего процесса
void read_string(pid_t child, unsigned long long addr, char *buffer, size_t size) {
    size_t i = 0;
    memset(buffer, 0, size);
    while (i < size - 1) {
        long data = ptrace(PTRACE_PEEKDATA, child, addr + i, NULL);
        if (errno != 0) {
            // Error reading memory, maybe invalid address. Stop.
            break;
        }
        memcpy(buffer + i, &data, sizeof(long));
        if (memchr(&data, '\0', sizeof(long)) != NULL) {
            break;
        }
        i += sizeof(long);
    }
    buffer[size - 1] = '\0';
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]);
        return 1;
    }

    pid_t child = fork();

    if (child == -1) {
        perror("fork");
        return 1;
    }

    if (child == 0) {
        // Дочерний процесс (tracee)
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execvp(argv[1], &argv[1]);
        perror("execvp");
        exit(1);
    } else {
        // Родительский процесс (tracer)
        int status;
        waitpid(child, &status, 0); // Wait for initial SIGTRAP from execvp
        ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD);

        printf("--- Syscall Trace for: %s ---\n", argv[1]);

        while(1) {
            // Continue to syscall entry
            if (ptrace(PTRACE_SYSCALL, child, 0, 0) == -1) { break; }
            if (waitpid(child, &status, 0) == -1) { break; }

            if (WIFEXITED(status)) {
                printf("--- Process exited with code %d ---\n", WEXITSTATUS(status));
                break;
            }
            if (WIFSIGNALED(status)) {
                printf("--- Process killed by signal %d ---\n", WTERMSIG(status));
                break;
            }

            // At syscall entry, get registers
            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, child, 0, &regs) == -1) { break; }
            long long syscall_num = regs.orig_rax;
            const char *name = get_syscall_name(syscall_num);

            printf("%s(", name);
            
            // Pretty print arguments for specific syscalls
            if (syscall_num == SYS_openat || syscall_num == SYS_access || syscall_num == SYS_readlink || syscall_num == SYS_newfstatat) {
                char path[1024];
                read_string(child, regs.rsi, path, sizeof(path));
                printf("dfd=%lld, path=\"%s\", flags=%#llx", regs.rdi, path, regs.rdx);
            } else if (syscall_num == SYS_write) {
                printf("fd=%lld, buf=%#llx, count=%lld", regs.rdi, regs.rsi, regs.rdx);
            } else if (syscall_num == SYS_read) {
                printf("fd=%lld, buf=%#llx, count=%lld", regs.rdi, regs.rsi, regs.rdx);
            } else if (syscall_num == SYS_mmap) {
                printf("addr=%#llx, len=%lld, prot=%#llx", regs.rdi, regs.rsi, regs.rdx);
            } else {
                printf("%lld, %lld, %lld", regs.rdi, regs.rsi, regs.rdx);
            }
            printf(")");
            fflush(stdout);

            // Continue to syscall exit
            if (ptrace(PTRACE_SYSCALL, child, 0, 0) == -1) { break; }
            if (waitpid(child, &status, 0) == -1) { break; }

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                printf(" = ? (process exited)\n");
                break;
            }

            // At syscall exit, get return value
            if (ptrace(PTRACE_GETREGS, child, 0, &regs) == -1) { break; }
            printf(" = %lld\n", regs.rax);
        }
    }
    return 0;
}
