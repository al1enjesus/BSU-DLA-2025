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
#include <signal.h>

#include "syscall_names.h"

pid_t child = -1;  // Глобальный идентификатор дочернего процесса

// Обработчик сигналов для корректного отсоединения
void handle_signal(int sig) {
    if (child > 0) {
        if (ptrace(PTRACE_DETACH, child, 0, 0) == -1) {
            perror("ptrace(PTRACE_DETACH)");
        } else {
            printf("\nDetached from child process %d due to signal %d\n", child, sig);
        }
    }
    exit(1);
}

// Функция для чтения строки из памяти дочернего процесса с проверкой ошибок
void read_string(pid_t child, unsigned long long addr, char *buffer, size_t size) {
    size_t i = 0;
    memset(buffer, 0, size);

    while (i < size - 1) {
        errno = 0;
        long data = ptrace(PTRACE_PEEKDATA, child, addr + i, NULL);
        if (data == -1 && errno != 0) {
            fprintf(stderr, "Error reading memory at %#llx: %s\n", addr + i, strerror(errno));
            buffer[i] = '\0';
            return;
        }

        size_t copy_size = sizeof(long);
        if (i + copy_size > size - 1) copy_size = size - 1 - i;

        memcpy(buffer + i, &data, copy_size);

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

    // Настройка обработчиков сигналов
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    child = fork();
    if (child == -1) {
        perror("fork");
        return 1;
    }

    if (child == 0) {
        // Дочерний процесс (tracee)
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("ptrace(PTRACE_TRACEME)");
            exit(1);
        }

        execvp(argv[1], &argv[1]);
        perror("execvp");  // Если execvp вернула управление, значит произошла ошибка
        exit(1);
    } else {
        // Родительский процесс (tracer)
        int status;
        if (waitpid(child, &status, 0) == -1) {
            perror("waitpid");
            return 1;
        }

        if (ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD) == -1) {
            perror("ptrace(PTRACE_SETOPTIONS)");
            return 1;
        }

        printf("--- Syscall Trace for: %s ---\n", argv[1]);

        while (1) {
            if (ptrace(PTRACE_SYSCALL, child, 0, 0) == -1) {
                perror("ptrace(PTRACE_SYSCALL) entry");
                break;
            }
            if (waitpid(child, &status, 0) == -1) {
                perror("waitpid");
                break;
            }

            if (WIFEXITED(status)) {
                printf("--- Process exited with code %d ---\n", WEXITSTATUS(status));
                break;
            }
            if (WIFSIGNALED(status)) {
                printf("--- Process killed by signal %d ---\n", WTERMSIG(status));
                break;
            }

            struct user_regs_struct regs;
            if (ptrace(PTRACE_GETREGS, child, 0, &regs) == -1) {
                perror("ptrace(PTRACE_GETREGS) entry");
                break;
            }
            long long syscall_num = regs.orig_rax;
            const char *name = get_syscall_name(syscall_num);

            printf("%s(", name);

            if (syscall_num == SYS_openat || syscall_num == SYS_access || syscall_num == SYS_readlink || syscall_num == SYS_newfstatat) {
                char path[1024];
                read_string(child, regs.rsi, path, sizeof(path));
                printf("dfd=%lld, path=\"%s\", flags=%#llx", regs.rdi, path, regs.rdx);
            } else if (syscall_num == SYS_write || syscall_num == SYS_read) {
                printf("fd=%lld, buf=%#llx, count=%lld", regs.rdi, regs.rsi, regs.rdx);
            } else if (syscall_num == SYS_mmap) {
                printf("addr=%#llx, len=%lld, prot=%#llx", regs.rdi, regs.rsi, regs.rdx);
            } else {
                printf("%lld, %lld, %lld", regs.rdi, regs.rsi, regs.rdx);
            }
            printf(")");
            fflush(stdout);

            if (ptrace(PTRACE_SYSCALL, child, 0, 0) == -1) {
                perror("ptrace(PTRACE_SYSCALL) exit");
                break;
            }
            if (waitpid(child, &status, 0) == -1) {
                perror("waitpid");
                break;
            }

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                printf(" = ? (process exited)\n");
                break;
            }

            if (ptrace(PTRACE_GETREGS, child, 0, &regs) == -1) {
                perror("ptrace(PTRACE_GETREGS) exit");
                break;
            }
            printf(" = %lld\n", regs.rax);
        }

        // Обеспечиваем корректное завершение дочернего процесса
        if (ptrace(PTRACE_DETACH, child, 0, 0) == -1 && errno != ESRCH) {
            perror("ptrace(PTRACE_DETACH) at program exit");
        }
    }

    return 0;
}
