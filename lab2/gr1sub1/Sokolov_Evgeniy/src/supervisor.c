#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_WORKERS 16
#define CONFIG_LINE_MAX 256
#define MAX_RESTARTS_PER_WINDOW 5
#define RESTART_WINDOW_SEC 30

typedef struct {
    int workers;
    long heavy_work_us;
    long heavy_sleep_us;
    long light_work_us;
    long light_sleep_us;
    int nice_values[MAX_WORKERS];
    int cpu_affinity[MAX_WORKERS];
} config_t;

typedef struct {
    pid_t pid;
    time_t last_restart_times[MAX_RESTARTS_PER_WINDOW];
    int restart_count;
    int nice_value;
    int cpu_affinity;
} worker_t;

static volatile sig_atomic_t shutdown_requested = 0;
static volatile sig_atomic_t reload_requested = 0;
static volatile sig_atomic_t toggle_light = 0;
static volatile sig_atomic_t toggle_heavy = 0;
static volatile sig_atomic_t child_died = 0;

static config_t config;
static worker_t workers[MAX_WORKERS];
static int num_workers = 0;

static void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            shutdown_requested = 1;
            break;
        case SIGHUP:
            reload_requested = 1;
            break;
        case SIGUSR1:
            toggle_light = 1;
            break;
        case SIGUSR2:
            toggle_heavy = 1;
            break;
        case SIGCHLD:
            child_died = 1;
            break;
    }
}

static void setup_signals(void) {
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);
}

static int parse_config(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen config");
        return -1;
    }
    
    // Установка значений по умолчанию
    config.workers = 3;
    config.heavy_work_us = 9000;
    config.heavy_sleep_us = 1000;
    config.light_work_us = 2000;
    config.light_sleep_us = 8000;
    
    for (int i = 0; i < MAX_WORKERS; i++) {
        config.nice_values[i] = 0;
        config.cpu_affinity[i] = -1;
    }
    
    char line[CONFIG_LINE_MAX];
    char section[64] = "";
    
    while (fgets(line, sizeof(line), fp)) {
        // Убираем комментарии и пробелы
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        
        // Убираем перенос строки
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Пропускаем пустые строки
        if (strlen(line) == 0) continue;
        
        // Обрабатываем секции
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                strcpy(section, line + 1);
            }
            continue;
        }
        
        // Обрабатываем параметры
        char *eq = strchr(line, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = line;
        char *value = eq + 1;
        
        // Убираем пробелы
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        
        if (strlen(section) == 0) {
            if (strcmp(key, "workers") == 0) {
                config.workers = atoi(value);
                if (config.workers > MAX_WORKERS) config.workers = MAX_WORKERS;
            }
        } else if (strcmp(section, "mode_heavy") == 0) {
            if (strcmp(key, "work_us") == 0) config.heavy_work_us = atol(value);
            else if (strcmp(key, "sleep_us") == 0) config.heavy_sleep_us = atol(value);
        } else if (strcmp(section, "mode_light") == 0) {
            if (strcmp(key, "work_us") == 0) config.light_work_us = atol(value);
            else if (strcmp(key, "sleep_us") == 0) config.light_sleep_us = atol(value);
        } else if (strcmp(section, "scheduling") == 0) {
            if (strcmp(key, "nice_values") == 0) {
                char *token = strtok(value, ",");
                int i = 0;
                while (token && i < MAX_WORKERS) {
                    config.nice_values[i] = atoi(token);
                    token = strtok(NULL, ",");
                    i++;
                }
            } else if (strcmp(key, "cpu_affinity") == 0) {
                char *token = strtok(value, ",");
                int i = 0;
                while (token && i < MAX_WORKERS) {
                    config.cpu_affinity[i] = atoi(token);
                    token = strtok(NULL, ",");
                    i++;
                }
            }
        }
    }
    
    fclose(fp);
    return 0;
}

static int can_restart_worker(int worker_idx) {
    time_t now = time(NULL);
    worker_t *w = &workers[worker_idx];
    
    // Удаляем старые записи о рестартах
    int valid_restarts = 0;
    for (int i = 0; i < w->restart_count; i++) {
        if (now - w->last_restart_times[i] < RESTART_WINDOW_SEC) {
            w->last_restart_times[valid_restarts] = w->last_restart_times[i];
            valid_restarts++;
        }
    }
    w->restart_count = valid_restarts;
    
    return w->restart_count < MAX_RESTARTS_PER_WINDOW;
}

static void record_restart(int worker_idx) {
    worker_t *w = &workers[worker_idx];
    if (w->restart_count < MAX_RESTARTS_PER_WINDOW) {
        w->last_restart_times[w->restart_count] = time(NULL);
        w->restart_count++;
    }
}

static void start_worker(int worker_idx) {
    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс - станет воркером
        char heavy_work_str[32], heavy_sleep_str[32];
        char light_work_str[32], light_sleep_str[32];
        char nice_str[32], cpu_str[32];
        
        snprintf(heavy_work_str, sizeof(heavy_work_str), "%ld", config.heavy_work_us);
        snprintf(heavy_sleep_str, sizeof(heavy_sleep_str), "%ld", config.heavy_sleep_us);
        snprintf(light_work_str, sizeof(light_work_str), "%ld", config.light_work_us);
        snprintf(light_sleep_str, sizeof(light_sleep_str), "%ld", config.light_sleep_us);
        snprintf(nice_str, sizeof(nice_str), "%d", workers[worker_idx].nice_value);
        snprintf(cpu_str, sizeof(cpu_str), "%d", workers[worker_idx].cpu_affinity);
        
        execl("./worker", "worker", 
              heavy_work_str, heavy_sleep_str,
              light_work_str, light_sleep_str,
              nice_str, cpu_str, NULL);
        
        perror("execl worker");
        exit(1);
    } else if (pid > 0) {
        workers[worker_idx].pid = pid;
        printf("[supervisor] Started worker %d: PID=%d, nice=%d, cpu=%d\n", 
               worker_idx, pid, workers[worker_idx].nice_value, workers[worker_idx].cpu_affinity);
    } else {
        perror("fork");
    }
}

static void start_all_workers(void) {
    num_workers = config.workers;
    
    for (int i = 0; i < num_workers; i++) {
        workers[i].nice_value = config.nice_values[i];
        workers[i].cpu_affinity = config.cpu_affinity[i];
        workers[i].restart_count = 0;
        start_worker(i);
    }
}

static void stop_all_workers(void) {
    printf("[supervisor] Stopping all workers...\n");
    
    // Посылаем SIGTERM всем воркерам
    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            kill(workers[i].pid, SIGTERM);
        }
    }
    
    // Ждём корректного завершения (до 5 секунд)
    time_t start_time = time(NULL);
    int all_stopped = 0;
    
    while (!all_stopped && (time(NULL) - start_time) < 5) {
        all_stopped = 1;
        for (int i = 0; i < num_workers; i++) {
            if (workers[i].pid > 0) {
                int status;
                pid_t result = waitpid(workers[i].pid, &status, WNOHANG);
                if (result == 0) {
                    all_stopped = 0;
                } else if (result == workers[i].pid) {
                    printf("[supervisor] Worker %d (PID=%d) stopped gracefully\n", i, workers[i].pid);
                    workers[i].pid = 0;
                }
            }
        }
        if (!all_stopped) usleep(100000); // 100ms
    }
    
    // Принудительно завершаем оставшихся
    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            printf("[supervisor] Force killing worker %d (PID=%d)\n", i, workers[i].pid);
            kill(workers[i].pid, SIGKILL);
            waitpid(workers[i].pid, NULL, 0);
            workers[i].pid = 0;
        }
    }
}

static void handle_child_death(void) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Найдём воркера по PID
        int worker_idx = -1;
        for (int i = 0; i < num_workers; i++) {
            if (workers[i].pid == pid) {
                worker_idx = i;
                break;
            }
        }
        
        if (worker_idx >= 0) {
            printf("[supervisor] Worker %d (PID=%d) died", worker_idx, pid);
            if (WIFEXITED(status)) {
                printf(" with exit code %d\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf(" by signal %d\n", WTERMSIG(status));
            } else {
                printf(" for unknown reason\n");
            }
            
            workers[worker_idx].pid = 0;
            
            // Перезапускаем, если не превышен лимит
            if (can_restart_worker(worker_idx)) {
                printf("[supervisor] Restarting worker %d\n", worker_idx);
                record_restart(worker_idx);
                start_worker(worker_idx);
            } else {
                printf("[supervisor] Worker %d restart limit exceeded\n", worker_idx);
            }
        }
    }
}

static void broadcast_signal_to_workers(int sig) {
    printf("[supervisor] Broadcasting signal %d to all workers\n", sig);
    for (int i = 0; i < num_workers; i++) {
        if (workers[i].pid > 0) {
            kill(workers[i].pid, sig);
        }
    }
}

static void reload_config(const char *config_file) {
    printf("[supervisor] Reloading configuration...\n");
    
    config_t old_config = config;
    if (parse_config(config_file) != 0) {
        printf("[supervisor] Failed to reload config, keeping old settings\n");
        config = old_config;
        return;
    }
    
    // Простая перезагрузка: останавливаем всех и запускаем заново
    stop_all_workers();
    start_all_workers();
    
    printf("[supervisor] Configuration reloaded\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config.ini>\n", argv[0]);
        return 1;
    }
    
    const char *config_file = argv[1];
    
    // Парсим конфигурацию
    if (parse_config(config_file) != 0) {
        return 1;
    }
    
    printf("[supervisor] Starting with %d workers\n", config.workers);
    
    // Устанавливаем обработчики сигналов
    setup_signals();
    
    // Запускаем воркеров
    start_all_workers();
    
    // Главный цикл супервизора
    while (!shutdown_requested) {
        if (child_died) {
            child_died = 0;
            handle_child_death();
        }
        
        if (reload_requested) {
            reload_requested = 0;
            reload_config(config_file);
        }
        
        if (toggle_light) {
            toggle_light = 0;
            broadcast_signal_to_workers(SIGUSR1);
        }
        
        if (toggle_heavy) {
            toggle_heavy = 0;
            broadcast_signal_to_workers(SIGUSR2);
        }
        
        usleep(100000); // 100ms
    }
    
    printf("[supervisor] Shutdown requested\n");
    stop_all_workers();
    printf("[supervisor] All workers stopped, exiting\n");
    
    return 0;
}