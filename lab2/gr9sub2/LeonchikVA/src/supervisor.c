#define _POSIX_C_SOURCE 200809L // Для kill/waitpid
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <errno.h>



#include <sys/resource.h> // <-- НОВЫЙ: Для setpriority
#include <sched.h>        // <-- НОВЫЙ: Для sched_setaffinity

#include "config.h"

// Макросы для таймаутов и ограничений
#define GRACEFUL_TIMEOUT_SEC 5
#define RESTART_LIMIT_COUNT 5
#define RESTART_LIMIT_PERIOD_SEC 30

// Структура для отслеживания воркеров
typedef struct {
    pid_t pid;
    int worker_id;
    time_t last_restart_time;
    int restart_count;
} worker_info_t;

// Глобальные флаги
volatile sig_atomic_t signal_received = 0;
volatile sig_atomic_t sigchld_received = 0;
volatile sig_atomic_t reload_requested = 0;
volatile sig_atomic_t shutdown_requested = 0;
volatile sig_atomic_t mode_signal = 0; 

// Глобальные переменные состояния
config_t current_config;
worker_info_t *workers = NULL;
int num_workers = 0;

// --- Signal Handling ---

static void supervisor_signal_handler(int sig) {
    signal_received = sig;
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            shutdown_requested = 1;
            break;
        case SIGHUP:
            reload_requested = 1;
            break;
        case SIGCHLD:
            sigchld_received = 1;
            break;
        case SIGUSR1:
            mode_signal = 1;
            break;
        case SIGUSR2:
            mode_signal = 2;
            break;
        default:
            break;
    }
}

static void setup_signals() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = supervisor_signal_handler;
    sa.sa_flags = SA_RESTART; 

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    
    signal(SIGPIPE, SIG_IGN);
}

// --- Worker Management ---

// Функция для запуска воркера
static pid_t launch_worker(int worker_idx) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        return -1;
    } else if (pid == 0) {
        // Код дочернего процесса (воркера)
        
        // --- ЧАСТЬ B: Установка Планирования ---
        int worker_nice, target_cpu;

        // Определяем CPU-аффинити (Чередуем CPU 0 и CPU 1)
        if ((worker_idx + 1) % 2 != 0) {
            target_cpu = current_config.affinity_cpu0; // CPU 0
        } else {
            target_cpu = current_config.affinity_cpu1; // CPU 1
        }

        // Определяем nice (Первая половина - nice=0, Вторая - nice=10)
        if ((worker_idx + 1) <= (current_config.workers / 2)) {
            worker_nice = current_config.nice_default; // nice=0
        } else {
            worker_nice = current_config.nice_low_prio; // nice=10
        }
        
        printf("[WORKER %d (PID %d)] Setting nice=%d and affinity to CPU %d\n", 
               worker_idx + 1, getpid(), worker_nice, target_cpu);
               
        // 1. Установка nice
        if (setpriority(PRIO_PROCESS, 0, worker_nice) == -1) {
             if (errno == EPERM) {
                 fprintf(stderr, "setpriority failed for PID %d (nice=%d): Permission denied (Need root or CAP_SYS_NICE)\n", 
                         getpid(), worker_nice);
             } else {
                 perror("setpriority failed");
             }
        }
        
        // 2. Установка CPU-аффинити
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(target_cpu, &cpuset);
        
        if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == -1) {
             fprintf(stderr, "sched_setaffinity failed for PID %d on CPU %d: %s\n", 
                     getpid(), target_cpu, strerror(errno));
        }

        // --- Конец Части B ---

        char worker_id_str[16];
        sprintf(worker_id_str, "%d", worker_idx + 1);
        
        // Запускаем worker
        execl("./worker", "./worker", worker_id_str, current_config.config_path, (char *)NULL);
        
        // execl возвращает ошибку только если не удалось запустить
        perror("execl worker failed");
        _exit(EXIT_FAILURE); 
    } else {
        // Код родительского процесса (супервизора)
        workers[worker_idx].pid = pid;
        workers[worker_idx].worker_id = worker_idx + 1;
        printf("Supervisor: Launched worker %d with PID %d\n", workers[worker_idx].worker_id, pid);
        return pid;
    }
}

static void start_workers(int count) {
    if (workers) free(workers);
    num_workers = count;
    workers = (worker_info_t *)calloc(num_workers, sizeof(worker_info_t));
    if (!workers) {
        perror("calloc failed");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < num_workers; i++) {
        workers[i].restart_count = 0;
        workers[i].last_restart_time = time(NULL);
        launch_worker(i);
    }
}

static void broadcast_signal(int sig) {
    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            printf("Supervisor: Sending signal %d to worker %d (PID %d)\n", sig, workers[i].worker_id, workers[i].pid);
            kill(workers[i].pid, sig);
        }
    }
}

static void handle_sigchld() {
    pid_t pid;
    int status;
    time_t now = time(NULL);

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        printf("Supervisor: Worker with PID %d terminated (Status: %d)\n", pid, status);

        int i;
        for (i = 0; i < num_workers; i++) {
            if (workers[i].pid == pid) {
                break;
            }
        }

        if (i == num_workers) {
            printf("Supervisor: WARNING: Unknown child PID %d terminated. Ignoring.\n", pid);
            continue;
        }

        workers[i].pid = 0;
        
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
             printf("Supervisor: Worker %d exited normally (exit code 0). No restart.\n", workers[i].worker_id);
             workers[i].restart_count = 0;
             workers[i].last_restart_time = now;
             continue;
        }
        
        printf("Supervisor: Worker %d exited abnormally or with error (Code: %d). Considering restart.\n", 
               workers[i].worker_id, WEXITSTATUS(status));
        
        if (now - workers[i].last_restart_time > RESTART_LIMIT_PERIOD_SEC) {
            workers[i].restart_count = 1;
            workers[i].last_restart_time = now;
        } else {
            workers[i].restart_count++;
        }

        if (workers[i].restart_count <= RESTART_LIMIT_COUNT) {
            printf("Supervisor: Restarting worker %d (Count: %d/%d)\n", 
                   workers[i].worker_id, workers[i].restart_count, RESTART_LIMIT_COUNT);
            launch_worker(i);
        } else {
            printf("Supervisor: WARNING: Worker %d exceeded restart limit (%d/%d). Aborting restart.\n", 
                   workers[i].worker_id, workers[i].restart_count, RESTART_LIMIT_COUNT);
        }
    }
    sigchld_received = 0;
}

static void graceful_shutdown() {
    printf("\nSupervisor: Initiating graceful shutdown (max %d seconds)...\n", GRACEFUL_TIMEOUT_SEC);

    broadcast_signal(SIGTERM);

    time_t start_time = time(NULL);
    int active_workers = num_workers;

    while (active_workers > 0 && (time(NULL) - start_time < GRACEFUL_TIMEOUT_SEC)) {
        pid_t pid;
        int status;
        pid = waitpid(-1, &status, WNOHANG);

        if (pid > 0) {
            active_workers--;
            printf("Supervisor: Worker PID %d shut down. Remaining: %d\n", pid, active_workers);
        } else if (pid == 0) {
            usleep(100000); 
        } else if (errno != ECHILD) {
             perror("waitpid error");
             break;
        }
    }

    if (active_workers > 0) {
        printf("Supervisor: WARNING: Timeout reached. Forcing termination of %d workers...\n", active_workers);
        broadcast_signal(SIGKILL);
        while (waitpid(-1, NULL, 0) > 0); 
    }
}

static void graceful_reload() {
    printf("\nSupervisor: Initiating graceful reload...\n");
    config_t new_config;
    
    if (read_config(current_config.config_path, &new_config) != 0) {
        fprintf(stderr, "Supervisor: Failed to read new config at %s. Aborting reload.\n", current_config.config_path);
        return;
    }
    printf("Supervisor: New config read successfully. Workers: %d -> %d\n", current_config.workers, new_config.workers);
    
    broadcast_signal(SIGTERM);

    time_t start_time = time(NULL);
    int old_active_workers = num_workers;
    
    while (old_active_workers > 0 && (time(NULL) - start_time < GRACEFUL_TIMEOUT_SEC)) {
         pid_t pid;
         int status;
         pid = waitpid(-1, &status, WNOHANG);
         if (pid > 0) {
            old_active_workers--;
         } else if (pid == 0) {
            usleep(100000); 
         } else if (errno != ECHILD) {
             perror("waitpid error during reload");
             break;
         }
    }
    
    if (workers) {
        free(workers);
        workers = NULL;
    }
    
    current_config = new_config;
    start_workers(current_config.workers);
    
    printf("Supervisor: Reload complete. New workers launched.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <config_path>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    if (read_config(argv[1], &current_config) != 0) {
        fprintf(stderr, "Could not read config at %s. Using defaults.\n", argv[1]);
        set_default_config(&current_config);
    }
    
    printf("Supervisor (PID %d) started with %d workers.\n", getpid(), current_config.workers);

    setup_signals();

    start_workers(current_config.workers);
    
    while (!shutdown_requested) {
        pause(); 
        
        if (sigchld_received) {
            handle_sigchld();
        }
        
        if (reload_requested) {
            graceful_reload();
            reload_requested = 0;
        }

        if (mode_signal) {
            int sig = (mode_signal == 1) ? SIGUSR1 : SIGUSR2;
            broadcast_signal(sig);
            mode_signal = 0;
        }
    }

    graceful_shutdown();

    if (workers) free(workers);

    printf("Supervisor: Exiting gracefully.\n");
    return EXIT_SUCCESS;
}