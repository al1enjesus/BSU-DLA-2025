#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

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
volatile sig_atomic_t child_exited = 0;

worker_info workers[MAX_WORKERS];
int num_workers = 3;
pthread_mutex_t workers_mutex = PTHREAD_MUTEX_INITIALIZER;

void sigchld_handler(int sig) {
    (void)sig;
    child_exited = 1;
}

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

    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
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
    int result = 0;

    pthread_mutex_lock(&workers_mutex);

    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid == pid) {
            if (now - workers[i].last_restart > RESTART_WINDOW) {
                workers[i].restarts = 0;
            }

            if (workers[i].restarts < MAX_RESTARTS) {
                workers[i].restarts++;
                workers[i].last_restart = now;
                result = 1;
            } else {
                printf("Supervisor: Too many restarts for worker %d\n", pid);
                result = 0;
            }
            break;
        }
    }

    pthread_mutex_unlock(&workers_mutex);
    return result;
}

void handle_child_exit() {
    pid_t pid;
    int status;

    pthread_mutex_lock(&workers_mutex);

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Supervisor: Worker %d exited with status %d\n", pid, WEXITSTATUS(status));

        if (keep_running && can_restart(pid)) {
            for (int i = 0; i < num_workers; i++) {
                if (workers[i].pid == pid) {
                    printf("Supervisor: Restarting worker...\n");
                    pid_t new_pid = start_worker();
                    if (new_pid > 0) {
                        workers[i].pid = new_pid;
                    } else {
                        fprintf(stderr, "Supervisor: Failed to restart worker\n");
                        workers[i].pid = -1;
                    }
                    break;
                }
            }
        }
    }

    if (pid == -1 && errno != ECHILD) {
        perror("waitpid failed");
    }

    pthread_mutex_unlock(&workers_mutex);
}

void graceful_shutdown() {
    printf("Supervisor: Starting graceful shutdown...\n");

    pthread_mutex_lock(&workers_mutex);

    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            printf("Supervisor: Sending SIGTERM to worker %d\n", workers[i].pid);
            if (kill(workers[i].pid, SIGTERM) == -1) {
                perror("kill failed");
            }
        }
    }

    pthread_mutex_unlock(&workers_mutex);

    time_t start = time(NULL);
    int workers_running;

    do {
        workers_running = 0;
        pthread_mutex_lock(&workers_mutex);

        for (int i = 0; i < num_workers; i++) {
            if (workers[i].pid > 0) {
                workers_running++;
            }
        }

        pthread_mutex_unlock(&workers_mutex);

        if (workers_running > 0 && (time(NULL) - start) < 5) {
            usleep(100000);
            handle_child_exit();
        }
    } while (workers_running > 0 && (time(NULL) - start) < 5);

    if (workers_running > 0) {
        printf("Supervisor: Force killing remaining workers\n");
        pthread_mutex_lock(&workers_mutex);

        for (int i = 0; i < num_workers; i++) {
            if (workers[i].pid > 0) {
                kill(workers[i].pid, SIGKILL);
            }
        }

        pthread_mutex_unlock(&workers_mutex);
    }

    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

void reload_workers() {
    printf("Supervisor: Reloading configuration and restarting workers...\n");

    pthread_mutex_lock(&workers_mutex);

    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            if (kill(workers[i].pid, SIGTERM) == -1) {
                perror("kill failed");
            }
        }
    }

    pthread_mutex_unlock(&workers_mutex);

    sleep(1);

    pthread_mutex_lock(&workers_mutex);

    for (int i = 0; i < num_workers; i++) {
        pid_t new_pid = start_worker();
        if (new_pid > 0) {
            workers[i].pid = new_pid;
            workers[i].restarts = 0;
            workers[i].last_restart = time(NULL);
        } else {
            fprintf(stderr, "Supervisor: Failed to start worker %d\n", i);
            workers[i].pid = -1;
        }
    }

    pthread_mutex_unlock(&workers_mutex);
}

void switch_workers_mode(int light_mode) {
    printf("Supervisor: Switching workers to %s mode\n",
           light_mode ? "LIGHT" : "HEAVY");

    pthread_mutex_lock(&workers_mutex);

    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            if (kill(workers[i].pid, light_mode ? SIGUSR1 : SIGUSR2) == -1) {
                perror("kill failed");
            }
        }
    }

    pthread_mutex_unlock(&workers_mutex);
}

int main(void) {
    printf("Supervisor started with PID: %d\n", getpid());
    setup_signals();

    pthread_mutex_lock(&workers_mutex);

    for (int i = 0; i < num_workers; i++) {
        pid_t pid = start_worker();
        if (pid > 0) {
            workers[i].pid = pid;
            workers[i].restarts = 0;
            workers[i].last_restart = time(NULL);
        } else {
            fprintf(stderr, "Supervisor: Failed to start initial worker %d\n", i);
            workers[i].pid = -1;
        }
    }

    pthread_mutex_unlock(&workers_mutex);

    printf("Supervisor: Started %d workers. Send signals to manage:\n", num_workers);
    printf("  SIGTERM/SIGINT - graceful shutdown\n");
    printf("  SIGHUP - reload configuration\n");
    printf("  SIGUSR1 - switch to light mode\n");
    printf("  SIGUSR2 - switch to heavy mode\n");
    printf("Supervisor PID: %d\n", getpid());

    while (keep_running) {
        if (child_exited) {
            child_exited = 0;
            handle_child_exit();
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

        usleep(100000);
    }

    graceful_shutdown();
    printf("Supervisor: Shutdown complete\n");

    pthread_mutex_destroy(&workers_mutex);
    return 0;
}