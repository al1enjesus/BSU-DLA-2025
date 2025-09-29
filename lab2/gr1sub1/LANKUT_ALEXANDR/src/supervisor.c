#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_WORKERS 10

int worker_pids[MAX_WORKERS];
int num_workers = 0;

void handle_sigchld(int sig) {
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    errno = saved_errno;
}

void handle_sigterm(int sig) {
    for (int i = 0; i < num_workers; i++) {
        if (kill(worker_pids[i], SIGTERM) == -1) {
            perror("kill");
        }
    }
    // Ждём завершения всех воркеров
    for (int i = 0; i < num_workers; i++) {
        if (waitpid(worker_pids[i], NULL, 0) == -1) {
            perror("waitpid");
        }
    }
    exit(0);
}

void spawn_workers(int n) {
    for (int i = 0; i < n; i++) {
        if (num_workers >= MAX_WORKERS) {
            fprintf(stderr, "Error: too many workers (max %d)\n", MAX_WORKERS);
            break;
        }
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            while (1) {
                printf("Worker %d running...\n", getpid());
                sleep(1);
            }
        } else {
            worker_pids[num_workers++] = pid;
        }
    }
}

int main() {
    struct sigaction sa;

    // Обрабатываем SIGCHLD
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction SIGCHLD");
        exit(1);
    }

    // Обрабатываем SIGTERM
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        exit(1);
    }

    // Порождаем воркеров
    spawn_workers(3);

    // Цикл супервизора
    while (1) {
        pause();
    }

    return 0;
}