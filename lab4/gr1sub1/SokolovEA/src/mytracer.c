#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>   // struct user_regs_struct
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "syscall_names.h"

// Чтение строки из памяти дочернего процесса
char *read_string(pid_t child, unsigned long long addr) {
    static char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    
    if (addr == 0) {
        strcpy(buffer, "(null)");
        return buffer;
    }
    
    for (int i = 0; i < 256; i += sizeof(long)) {
        errno = 0;
        long data = ptrace(PTRACE_PEEKDATA, child, addr + i, NULL);
        if (errno != 0) {
            strcpy(buffer, "(unreadable)");
            return buffer;
        }
        
        memcpy(buffer + i, &data, sizeof(long));
        
        // Проверяем, есть ли нулевой символ в загруженных данных
        for (int j = 0; j < sizeof(long); j++) {
            if (((char*)&data)[j] == '\0') {
                return buffer;
            }
        }
    }
    
    buffer[255] = '\0';
    return buffer;
}

// Форматирование аргументов для известных системных вызовов
void print_syscall_args(long long syscall_num, struct user_regs_struct *regs_enter, long long retval) {
    const char *name = get_syscall_name(syscall_num);
    
    printf("%s(", name);
    
    switch (syscall_num) {
        case 39:  // getpid()
            printf(") = %lld\n", retval);
            break;
            
        case 1:   // write(fd, buf, count)
            printf("fd=%lld, buf=0x%llx, count=%lld) = %lld\n",
                   regs_enter->rdi, regs_enter->rsi, regs_enter->rdx, retval);
            break;
            
        case 0:   // read(fd, buf, count)
            printf("fd=%lld, buf=0x%llx, count=%lld) = %lld\n",
                   regs_enter->rdi, regs_enter->rsi, regs_enter->rdx, retval);
            break;
            
        case 257: { // openat(dfd, filename, flags)
            pid_t child = 0; // Нужно передать child_pid
            printf("dirfd=%lld, pathname=0x%llx, flags=0x%llx) = %lld\n",
                   regs_enter->rdi, regs_enter->rsi, regs_enter->rdx, retval);
            break;
        }
        
        case 3:   // close(fd)
            printf("fd=%lld) = %lld\n", regs_enter->rdi, retval);
            break;
            
        case 59:  // execve(filename, argv, envp)
            printf("filename=0x%llx, argv=0x%llx, envp=0x%llx) = %lld\n",
                   regs_enter->rdi, regs_enter->rsi, regs_enter->rdx, retval);
            break;
            
        case 96:  // gettimeofday(tv, tz)
            printf("tv=0x%llx, tz=0x%llx) = %lld\n",
                   regs_enter->rdi, regs_enter->rsi, retval);
            break;
            
        case 9:   // mmap(addr, length, prot, flags, fd, offset)
            printf("addr=0x%llx, length=%lld, prot=0x%llx, flags=0x%llx, fd=%lld, offset=%lld) = 0x%llx\n",
                   regs_enter->rdi, regs_enter->rsi, regs_enter->rdx, 
                   regs_enter->r10, regs_enter->r8, regs_enter->r9, retval);
            break;
            
        default:
            // Общий формат для остальных syscalls
            printf("0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx) = %lld\n",
                   regs_enter->rdi, regs_enter->rsi, regs_enter->rdx,
                   regs_enter->r10, regs_enter->r8, regs_enter->r9, retval);
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program> [args...]\n", argv[0]);
        fprintf(stderr, "Упрощённый strace через ptrace\n");
        return 1;
    }

    pid_t child = fork();

    if (child == -1) {
        perror("fork");
        return 1;
    }

    if (child == 0) {
        // ═══════════════════════════════════════════════════════════
        // ДОЧЕРНИЙ ПРОЦЕСС (tracee)
        // ═══════════════════════════════════════════════════════════

        // Шаг 1: Разрешаем родителю трассировать нас
        // После этого вызова каждый execve() будет вызывать SIGTRAP
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("ptrace(PTRACE_TRACEME)");
            exit(1);
        }

        // Шаг 2: Запускаем целевую программу (например, ls)
        // execvp() заменяет наш процесс новой программой
        // argv[1] = имя программы, &argv[1] = массив аргументов
        execvp(argv[1], &argv[1]);

        // Если дошли сюда — execvp() провалился
        perror("execvp");
        exit(1);

    } else {
        // ═══════════════════════════════════════════════════════════
        // РОДИТЕЛЬСКИЙ ПРОЦЕСС (tracer)
        // ═══════════════════════════════════════════════════════════

        int status;
        int syscall_count = 0;

        // Шаг 1: Ждём первой остановки дочернего процесса
        // Она происходит сразу после execvp() из-за PTRACE_TRACEME
        waitpid(child, &status, 0);

        // Шаг 2: Настраиваем опции трассировки
        // PTRACE_O_TRACESYSGOOD: при остановке на syscall ставим бит 0x80 в status
        // Это позволяет отличить остановку на syscall от обычного сигнала
        ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD);

        printf("[TRACER] Started tracing PID %d (%s)\n", child, argv[1]);
        printf("==========================================\n");

        // Шаг 3: Основной цикл трассировки
        while (1) {
            // 3.1: Продолжить выполнение до ВХОДА в следующий syscall
            if (ptrace(PTRACE_SYSCALL, child, 0, 0) == -1) {
                perror("ptrace(PTRACE_SYSCALL)");
                break;
            }

            // 3.2: Ждём остановки (дочерний остановился ПЕРЕД syscall)
            if (waitpid(child, &status, 0) == -1) {
                perror("waitpid");
                break;
            }

            // 3.3: Проверяем, завершился ли процесс
            if (WIFEXITED(status)) {
                printf("[TRACER] Process exited with code %d\n",
                       WEXITSTATUS(status));
                break;
            }

            if (WIFSIGNALED(status)) {
                printf("[TRACER] Process killed by signal %d\n",
                       WTERMSIG(status));
                break;
            }

            // 3.4: Читаем регистры процесса (ВХОД в syscall)
            struct user_regs_struct regs_enter;
            if (ptrace(PTRACE_GETREGS, child, 0, &regs_enter) == -1) {
                perror("ptrace(PTRACE_GETREGS)");
                break;
            }

            long long syscall_num = regs_enter.orig_rax;

            // 3.5: Продолжить выполнение до ВЫХОДА из syscall
            if (ptrace(PTRACE_SYSCALL, child, 0, 0) == -1) {
                perror("ptrace(PTRACE_SYSCALL)");
                break;
            }

            // 3.6: Ждём остановки (дочерний остановился ПОСЛЕ syscall)
            if (waitpid(child, &status, 0) == -1) {
                perror("waitpid");
                break;
            }

            if (WIFEXITED(status)) {
                printf("[TRACER] Process exited with code %d\n",
                       WEXITSTATUS(status));
                break;
            }

            if (WIFSIGNALED(status)) {
                printf("[TRACER] Process killed by signal %d\n",
                       WTERMSIG(status));
                break;
            }

            // 3.7: Читаем регистры процесса (ВЫХОД из syscall)
            struct user_regs_struct regs_exit;
            if (ptrace(PTRACE_GETREGS, child, 0, &regs_exit) == -1) {
                perror("ptrace(PTRACE_GETREGS)");
                break;
            }

            long long retval = regs_exit.rax;

            // 3.8: Выводим информацию о системном вызове
            printf("%4d. ", ++syscall_count);
            print_syscall_args(syscall_num, &regs_enter, retval);
        }

        printf("\n[TRACER] Total syscalls traced: %d\n", syscall_count);
    }

    return 0;
}