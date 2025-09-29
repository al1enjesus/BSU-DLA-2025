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
        kill(worker_pids[i], SIGTERM);
    }
    exit(0);
}

void spawn_workers(int n) {
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
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
    sigaction(SIGCHLD, &sa, NULL);

    // Обрабатываем SIGTERM
    signal(SIGTERM, handle_sigterm);

    // Порождаем воркеров
    spawn_workers(3);

    // Цикл супервизора
    while (1) {
        pause();
    }

    return 0;
}