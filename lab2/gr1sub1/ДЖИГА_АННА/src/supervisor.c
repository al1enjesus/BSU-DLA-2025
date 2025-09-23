#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <sched.h>

#define MAX_WORKERS 8

static int n_workers = 3;
static pid_t workers[MAX_WORKERS];
static volatile sig_atomic_t shutdown_requested = 0;
static volatile sig_atomic_t reload_requested = 0;
static volatile sig_atomic_t sigusr_mode = 0;

void spawn_worker(int idx) {
    pid_t pid = fork();
    if (pid == 0) {
        execl("./worker", "./worker", NULL);
        perror("execl worker");
        _exit(1);
    } else if (pid > 0) {
        workers[idx] = pid;
        if (idx % 2 == 0) {
            if (setpriority(PRIO_PROCESS, pid, 10) == -1) {
                perror("setpriority");
            }
        } else {
            if (setpriority(PRIO_PROCESS, pid, 0) == -1) {
                perror("setpriority");
            }
        }
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(idx % 2, &mask);
        if (sched_setaffinity(pid, sizeof(mask), &mask) == -1) {
            perror("sched_setaffinity");
        }
        printf("[Supervisor] started worker %d (pid=%d)\n", idx, pid);
    }
}

void sigchld_handler(int signo) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (shutdown_requested) {
            continue;
        }
        for (int i = 0; i < n_workers; i++) {
            if (workers[i] == pid) {
                printf("[Supervisor] worker %d (pid=%d) exited, restarting...\n", i, pid);
                spawn_worker(i);
            }
        }
    }
}


void sigterm_handler(int signo) { shutdown_requested = 1; }
void sighup_handler(int signo) { reload_requested = 1; }
void sigusr1_handler(int signo) { sigusr_mode = 1; }
void sigusr2_handler(int signo) { sigusr_mode = 2; }

int main() {
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);
    signal(SIGHUP, sighup_handler);
    signal(SIGUSR1, sigusr1_handler);
    signal(SIGUSR2, sigusr2_handler);

    for (int i = 0; i < n_workers; i++) spawn_worker(i);

    while (!shutdown_requested) {
        sleep(1);
        if (reload_requested) {
            printf("[Supervisor] reload requested\n");
            for (int i = 0; i < n_workers; i++) {
                kill(workers[i], SIGTERM);
                waitpid(workers[i], NULL, 0);
                spawn_worker(i);
            }
            reload_requested = 0;
        }
        if (sigusr_mode == 1) {
            for (int i = 0; i < n_workers; i++) kill(workers[i], SIGUSR1);
            sigusr_mode = 0;
        } else if (sigusr_mode == 2) {
            for (int i = 0; i < n_workers; i++) kill(workers[i], SIGUSR2);
            sigusr_mode = 0;
        }
    }

    printf("[Supervisor] shutting down\n");
    for (int i = 0; i < n_workers; i++) kill(workers[i], SIGTERM);
    for (int i = 0; i < n_workers; i++) waitpid(workers[i], NULL, 0);
    return 0;
}
