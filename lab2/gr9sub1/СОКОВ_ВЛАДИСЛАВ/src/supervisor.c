#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_WORKERS 10
#define CONFIG_FILE "src/config.ini"

// Глобальные флаги сигналов
static volatile sig_atomic_t stop_requested = 0;
static volatile sig_atomic_t reload_requested = 0;
static volatile sig_atomic_t mode_signal = 0; // 0=none, 1=USR1 (light), 2=USR2 (heavy)

// Структура конфигурации
typedef struct {
    int workers_count;
    int heavy_work;
    int heavy_sleep;
    int light_work;
    int light_sleep;
} Config;

// Хранение PID воркеров
static pid_t workers[MAX_WORKERS];
static int current_worker_count = 0;

// Для ограничения частоты рестартов (5 за 30 сек)
#define RESTART_WINDOW 30
#define MAX_RESTARTS 5
static time_t restart_times[MAX_RESTARTS];
static int restart_idx = 0;

// Обработчики сигналов
void handle_sigterm(int sig) { (void)sig; stop_requested = 1; }
void handle_sighup(int sig) { (void)sig; reload_requested = 1; }
void handle_sigusr1(int sig) { (void)sig; mode_signal = 1; }
void handle_sigusr2(int sig) { (void)sig; mode_signal = 2; }

// Чтение конфига (простой парсер)
int load_config(Config *cfg) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return -1;

    // Дефолтные значения
    cfg->workers_count = 2;
    cfg->heavy_work = 9000; cfg->heavy_sleep = 1000;
    cfg->light_work = 2000; cfg->light_sleep = 8000;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "workers=", 8) == 0) cfg->workers_count = atoi(line + 8);
        else if (strncmp(line, "heavy_work=", 11) == 0) cfg->heavy_work = atoi(line + 11);
        else if (strncmp(line, "heavy_sleep=", 12) == 0) cfg->heavy_sleep = atoi(line + 12);
        else if (strncmp(line, "light_work=", 11) == 0) cfg->light_work = atoi(line + 11);
        else if (strncmp(line, "light_sleep=", 12) == 0) cfg->light_sleep = atoi(line + 12);
    }
    fclose(f);
    
    if (cfg->workers_count > MAX_WORKERS) cfg->workers_count = MAX_WORKERS;
    if (cfg->workers_count < 1) cfg->workers_count = 1;
    return 0;
}

// Проверка лимита рестартов
int can_restart() {
    time_t now = time(NULL);
    // Очищаем старые записи
    if (now - restart_times[restart_idx] > RESTART_WINDOW) {
        restart_times[restart_idx] = now;
        restart_idx = (restart_idx + 1) % MAX_RESTARTS;
        return 1; // Можно рестартить
    }
    
    // Проверяем самый старый слот в окне
    int oldest_idx = (restart_idx + 1) % MAX_RESTARTS;
    if (now - restart_times[oldest_idx] <= RESTART_WINDOW && restart_times[oldest_idx] != 0) {
        return 0; // Слишком много рестартов
    }

    restart_times[restart_idx] = now;
    restart_idx = (restart_idx + 1) % MAX_RESTARTS;
    return 1;
}

// Запуск одного воркера
pid_t spawn_worker(Config *cfg) {
    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс
        char arg_hw[32], arg_hs[32], arg_lw[32], arg_ls[32];
        sprintf(arg_hw, "%d", cfg->heavy_work);
        sprintf(arg_hs, "%d", cfg->heavy_sleep);
        sprintf(arg_lw, "%d", cfg->light_work);
        sprintf(arg_ls, "%d", cfg->light_sleep);

        // Запускаем cpu_burn с параметрами из конфига
        char *argv[] = {
            "./src/cpu_burn",
            "--work-us", arg_hw,
            "--sleep-us", arg_hs,
            "--light-work-us", arg_lw,
            "--light-sleep-us", arg_ls,
            NULL
        };
        execvp(argv[0], argv);
        perror("execvp failed");
        exit(1);
    }
    return pid;
}

// Остановка всех воркеров
void stop_all_workers() {
    for (int i = 0; i < MAX_WORKERS; i++) {
        if (workers[i] > 0) {
            kill(workers[i], SIGTERM);
        }
    }
    // Ждем завершения
    while(wait(NULL) > 0);
    memset(workers, 0, sizeof(workers));
    current_worker_count = 0;
}

int main() {
    // Настройка обработки сигналов
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_sighup;
    sigaction(SIGHUP, &sa, NULL); // Для reload

    sa.sa_handler = handle_sigusr1;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = handle_sigusr2;
    sigaction(SIGUSR2, &sa, NULL);

    Config cfg;
    if (load_config(&cfg) != 0) {
        fprintf(stderr, "Failed to load config, using defaults\n");
    }

    printf("[Supervisor] Started PID=%d. Workers=%d\n", getpid(), cfg.workers_count);

    // Первый запуск воркеров
    memset(workers, 0, sizeof(workers));
    memset(restart_times, 0, sizeof(restart_times));

    for (int i = 0; i < cfg.workers_count; i++) {
        workers[i] = spawn_worker(&cfg);
        current_worker_count++;
    }

    while (!stop_requested) {
        // 1. Обработка Reload (SIGHUP)
        if (reload_requested) {
            reload_requested = 0;
            printf("[Supervisor] Reloading config...\n");
            stop_all_workers();
            load_config(&cfg);
            for (int i = 0; i < cfg.workers_count; i++) {
                workers[i] = spawn_worker(&cfg);
                current_worker_count++;
            }
            printf("[Supervisor] Reload complete.\n");
        }

        // 2. Обработка переключения режимов (SIGUSR1/2)
        if (mode_signal != 0) {
            int sig = (mode_signal == 1) ? SIGUSR1 : SIGUSR2;
            const char* sname = (mode_signal == 1) ? "LIGHT" : "HEAVY";
            printf("[Supervisor] Broadcasting %s mode to workers\n", sname);
            for (int i = 0; i < MAX_WORKERS; i++) {
                if (workers[i] > 0) kill(workers[i], sig);
            }
            mode_signal = 0;
        }

        // 3. Проверка статуса детей (Zombie reaping) и рестарт
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            // Найти индекс умершего
            int idx = -1;
            for (int i = 0; i < MAX_WORKERS; i++) {
                if (workers[i] == pid) {
                    idx = i;
                    workers[i] = 0;
                    break;
                }
            }

            if (!stop_requested && !reload_requested && idx != -1) {
                printf("[Supervisor] Worker %d died unexpectedly.\n", pid);
                if (can_restart()) {
                    printf("[Supervisor] Restarting worker slot %d...\n", idx);
                    workers[idx] = spawn_worker(&cfg);
                } else {
                    printf("[Supervisor] Rate limit exceeded. Not restarting yet.\n");
                }
            }
        }

        // Пауза, чтобы не грузить CPU в цикле (supervisor должен спать большую часть времени)
        usleep(100000); 
    }

    printf("[Supervisor] Stopping...\n");
    stop_all_workers();
    printf("[Supervisor] Byte.\n");
    return 0;
}