#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include "config.h"

#define MAX_WORKERS 10
#define MAX_RESTARTS 5
#define RESTART_WINDOW 30

typedef struct {
    pid_t pid;
    time_t start_time;
    int restart_count;
    time_t last_restart;
    int worker_id;
} worker_info_t;

volatile sig_atomic_t supervisor_running = 1;
volatile sig_atomic_t reload_requested = 0;
volatile sig_atomic_t mode_switch_requested = 0;
volatile sig_atomic_t switch_to_light = 0;

worker_info_t workers[MAX_WORKERS];
config_t supervisor_config;

void signal_handler(int sig) {
    switch(sig) {
        case SIGTERM:
        case SIGINT:
            supervisor_running = 0;
            printf("Supervisor: Received shutdown signal\n");
            break;
        case SIGHUP:
            reload_requested = 1;
            printf("Supervisor: Received reload signal\n");
            break;
        case SIGUSR1:
            mode_switch_requested = 1;
            switch_to_light = 1;
            printf("Supervisor: Switching workers to LIGHT mode\n");
            break;
        case SIGUSR2:
            mode_switch_requested = 1;
            switch_to_light = 0;
            printf("Supervisor: Switching workers to HEAVY mode\n");
            break;
    }
}

void setup_signals() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    
    // Для SIGCHLD используем SA_NOCLDSTOP
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

void set_worker_scheduling(pid_t pid, int nice_value, int cpu_affinity) {
    // Установка nice значения
    if (nice_value != 0) {
        setpriority(PRIO_PROCESS, pid, nice_value);
        printf("Supervisor: Set nice=%d for worker %d\n", nice_value, pid);
    }
    
    // Установка CPU аффинити
#ifdef linux
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpu_affinity % sysconf(_SC_NPROCESSORS_ONLN), &mask);
    
    if (sched_setaffinity(pid, sizeof(mask), &mask) == 0) {
        printf("Supervisor: Set CPU affinity=%d for worker %d\n", cpu_affinity, pid);
    }
#endif
}

int can_restart(worker_info_t *worker) {
    time_t now = time(NULL);
    
    // Если это первый запуск или прошло больше окна - разрешаем
    if (worker->restart_count == 0 || (now - worker->last_restart) >= RESTART_WINDOW) {
        worker->restart_count = 0; // Сброс счетчика
        return 1;
    }
    
    // Проверяем лимит рестартов в окне
    return (worker->restart_count < MAX_RESTARTS);
}

void update_restart_stats(worker_info_t *worker) {
    time_t now = time(NULL);
    if (now - worker->last_restart < RESTART_WINDOW) {
        worker->restart_count++;
    } else {
        worker->restart_count = 1;
    }
    worker->last_restart = now;
}

pid_t start_worker(int worker_id) {
    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс
        char id_str[16];
        sprintf(id_str, "%d", worker_id);
        execl("./worker", "worker", id_str, NULL);
        perror("execl failed");
        exit(1);
    }
    return pid;
}

void stop_worker(pid_t pid) {
    printf("Supervisor: Stopping worker %d\n", pid);
    kill(pid, SIGTERM);
    
    // Ждем завершения с таймаутом 3 секунды (вместо 5)
    int waited = 0;
    for (int i = 0; i < 30; i++) { // 30 * 100ms = 3 секунды
        if (kill(pid, 0) != 0) {
            printf("Supervisor: Worker %d exited gracefully\n", pid);
            return; // Процесс завершился
        }
        usleep(100000); // 100ms
        waited++;
    }
    
    // Принудительное завершение если не завершился за 3 секунды
    if (kill(pid, 0) == 0) {
        printf("Supervisor: Force killing worker %d (timeout after %d attempts)\n", pid, waited);
        kill(pid, SIGKILL);
        usleep(50000); // Даем время для обработки SIGKILL
    }
}

void stop_all_workers() {
    printf("Supervisor: Stopping all workers...\n");
    for (int i = 0; i < supervisor_config.workers_count; i++) {
        if (workers[i].pid > 0) {
            stop_worker(workers[i].pid);
        }
    }
}
void restart_worker(int index) {
    if (workers[index].pid > 0) {
        printf("Supervisor: Worker %d crashed, restarting...\n", workers[index].pid);
        stop_worker(workers[index].pid);
    }
    
    if (can_restart(&workers[index])) {
        workers[index].pid = start_worker(workers[index].worker_id);
        workers[index].start_time = time(NULL);
        update_restart_stats(&workers[index]);
        
        // Устанавливаем планирование для задания B
        if (workers[index].worker_id % 2 == 0) {
            set_worker_scheduling(workers[index].pid, 0, 0); // Высокий приоритет на CPU 0
        } else {
            set_worker_scheduling(workers[index].pid, 10, 1); // Низкий приоритет на CPU 1
        }
    } else {
        printf("Supervisor: Restart limit exceeded for worker %d\n", workers[index].worker_id);
        workers[index].pid = -1;
    }
}

void switch_workers_mode(int to_light) {
    int signal = to_light ? SIGUSR1 : SIGUSR2;
    for (int i = 0; i < supervisor_config.workers_count; i++) {
        if (workers[i].pid > 0) {
            kill(workers[i].pid, signal);
        }
    }
}

void reload_workers() {
    printf("Supervisor: Reloading configuration...\n");
    config_t old_config = supervisor_config;
    reload_config(&supervisor_config);
    
    // Останавливаем лишних воркеров
    for (int i = supervisor_config.workers_count; i < old_config.workers_count; i++) {
        if (workers[i].pid > 0) {
            stop_worker(workers[i].pid);
            workers[i].pid = -1;
        }
    }
    
    // Запускаем новых воркеров если нужно больше
    for (int i = old_config.workers_count; i < supervisor_config.workers_count; i++) {
        workers[i].pid = start_worker(i);
        workers[i].worker_id = i;
        workers[i].start_time = time(NULL);
        workers[i].restart_count = 0;
        
        if (i % 2 == 0) {
            set_worker_scheduling(workers[i].pid, 0, 0);
        } else {
            set_worker_scheduling(workers[i].pid, 10, 1);
        }
    }
}

void check_workers() {
    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < supervisor_config.workers_count; i++) {
            if (workers[i].pid == pid) {
                if (WIFEXITED(status)) {
                    printf("Supervisor: Worker[%d] %d exited with status %d\n", 
                           workers[i].worker_id, pid, WEXITSTATUS(status));
                } else if (WIFSIGNALED(status)) {
                    printf("Supervisor: Worker[%d] %d killed by signal %d\n", 
                           workers[i].worker_id, pid, WTERMSIG(status));
                }
                
                if (!supervisor_running) {
                    // При shutdown не перезапускаем
                    workers[i].pid = -1;
                } else {
                    restart_worker(i);
                }
                break;
            }
        }
    }
}

void initialize_workers() {
    for (int i = 0; i < supervisor_config.workers_count; i++) {
        workers[i].pid = start_worker(i);
        workers[i].worker_id = i;
        workers[i].start_time = time(NULL);
        workers[i].restart_count = 0;
        workers[i].last_restart = 0;
        
        // Устанавливаем планирование для задания B
        if (i % 2 == 0) {
            set_worker_scheduling(workers[i].pid, 0, 0); // Высокий приоритет на CPU 0
        } else {
            set_worker_scheduling(workers[i].pid, 10, 1); // Низкий приоритет на CPU 1
        }
        
        usleep(100000); // Пауза между запусками
    }
}
int main() {
    printf("=== Supervisor started (PID: %d) ===\n", getpid());
    
    setup_signals();
    load_config(&supervisor_config);
    
    // Инициализация воркеров
    initialize_workers();
    
    printf("Supervisor: Started %d workers\n", supervisor_config.workers_count);
    printf("Workers with even IDs: nice=0, CPU=0\n");
    printf("Workers with odd IDs: nice=10, CPU=1\n");
    
    while (supervisor_running) {
        // Проверяем состояние воркеров
        check_workers();
        
        // Обрабатываем запросы
        if (reload_requested) {
            reload_workers();
            reload_requested = 0;
        }
        
        if (mode_switch_requested) {
            switch_workers_mode(switch_to_light);
            mode_switch_requested = 0;
        }
        
        usleep(100000); // 100ms
    }
    
    // Graceful shutdown
    printf("Supervisor: Initiating graceful shutdown...\n");
    stop_all_workers();
    
    // Ждем завершения всех воркеров
    sleep(1);
    check_workers();
    
    printf("Supervisor: Shutdown completed\n");
    return 0;
}
