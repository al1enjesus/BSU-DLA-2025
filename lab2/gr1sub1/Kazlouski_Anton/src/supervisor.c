#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define MAX_WORKERS 10
#define MAX_RESTARTS 5
#define RESTART_WINDOW 30

typedef struct {
    pid_t pid;
    int restarts;
    time_t last_restart;
} worker_info;

volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t reload_config = 0;
volatile sig_atomic_t switch_to_light = 0;
volatile sig_atomic_t switch_to_heavy = 0;

worker_info workers[MAX_WORKERS];
int num_workers = 3;
int worker_pids[MAX_WORKERS];

void signal_handler(int sig) {
    switch(sig) {
        case SIGTERM:
        case SIGINT:
            keep_running = 0;
            break;
        case SIGHUP:
            reload_config = 1;
            break;
        case SIGUSR1:
            switch_to_light = 1;
            break;
        case SIGUSR2:
            switch_to_heavy = 1;
            break;
    }
}

void setup_signals() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    signal(SIGCHLD, SIG_DFL);
}

pid_t start_worker() {
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {"./worker", NULL};
        execvp("./worker", args);
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        printf("Supervisor: Started worker with PID %d\n", pid);
        return pid;
    } else {
        perror("fork failed");
        return -1;
    }
}

int can_restart(pid_t pid) {
    time_t now = time(NULL);

    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid == pid) {
            if (now - workers[i].last_restart > RESTART_WINDOW) {
                workers[i].restarts = 0;
            }

            if (workers[i].restarts < MAX_RESTARTS) {
                workers[i].restarts++;
                workers[i].last_restart = now;
                return 1;
            } else {
                printf("Supervisor: Too many restarts for worker %d\n", pid);
                return 0;
            }
        }
    }
    return 1;
}

void graceful_shutdown() {
    printf("Supervisor: Starting graceful shutdown...\n");

    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            printf("Supervisor: Sending SIGTERM to worker %d\n", workers[i].pid);
            kill(workers[i].pid, SIGTERM);
        }
    }

    time_t start = time(NULL);
    int workers_running = num_workers;

    while (workers_running > 0 && (time(NULL) - start) < 5) {
        pid_t pid = waitpid(-1, NULL, WNOHANG);
        if (pid > 0) {
            printf("Supervisor: Worker %d terminated\n", pid);
            workers_running--;
        } else if (pid == -1 && errno == ECHILD) {
            break;
        }
        usleep(100000);
    }

    if (workers_running > 0) {
        printf("Supervisor: Force killing remaining workers\n");
        for (int i = 0; i < num_workers; i++) {
            if (workers[i].pid > 0) {
                kill(workers[i].pid, SIGKILL);
            }
        }
    }

    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

void reload_workers() {
    printf("Supervisor: Reloading configuration and restarting workers...\n");

    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            kill(workers[i].pid, SIGTERM);
        }
    }

    sleep(1);

    for (int i = 0; i < num_workers; i++) {
        workers[i].pid = start_worker();
        workers[i].restarts = 0;
        workers[i].last_restart = time(NULL);
    }
}

void switch_workers_mode(int light_mode) {
    printf("Supervisor: Switching workers to %s mode\n",
           light_mode ? "LIGHT" : "HEAVY");

    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            kill(workers[i].pid, light_mode ? SIGUSR1 : SIGUSR2);
        }
    }
}

int main() {
    printf("Supervisor started with PID: %d\n", getpid());
    setup_signals();

    for (int i = 0; i < num_workers; i++) {
        workers[i].pid = start_worker();
        workers[i].restarts = 0;
        workers[i].last_restart = time(NULL);
    }

    printf("Supervisor: Started %d workers. Send signals to manage:\n", num_workers);
    printf("  SIGTERM/SIGINT - graceful shutdown\n");
    printf("  SIGHUP - reload configuration\n");
    printf("  SIGUSR1 - switch to light mode\n");
    printf("  SIGUSR2 - switch to heavy mode\n");

    while (keep_running) {
        pid_t pid;
        int status;

        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            printf("Supervisor: Worker %d exited with status %d\n", pid, status);

            if (keep_running && can_restart(pid)) {
                for (int i = 0; i < num_workers; i++) {
                    if (workers[i].pid == pid) {
                        workers[i].pid = start_worker();
                        break;
                    }
                }
            }
        }

        if (reload_config) {
            reload_config = 0;
            reload_workers();
        }

        if (switch_to_light) {
            switch_to_light = 0;
            switch_workers_mode(1);
        }

        if (switch_to_heavy) {
            switch_to_heavy = 0;
            switch_workers_mode(0);
        }

        usleep(100000); // 100ms
    }

    graceful_shutdown();
    printf("Supervisor: Shutdown complete\n");
    return 0;
} 