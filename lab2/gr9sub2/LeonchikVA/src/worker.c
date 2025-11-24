#define _POSIX_C_SOURCE 200809L // Для usleep

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "config.h"

// Глобальные флаги и переменные
volatile sig_atomic_t running = 1;
volatile sig_atomic_t mode_changed = 0;
config_t current_config;
mode_params_t current_mode;
char *mode_name = "HEAVY";
long long cycles_count = 0;

// Функция-обработчик сигналов
static void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            // Корректное завершение
            running = 0;
            break;
        case SIGUSR1:
            // Переключение на ЛЕГКИЙ режим
            mode_changed = 1;
            current_mode = current_config.light;
            mode_name = "LIGHT";
            break;
        case SIGUSR2:
            // Переключение на ТЯЖЕЛЫЙ режим
            mode_changed = 1;
            current_mode = current_config.heavy;
            mode_name = "HEAVY";
            break;
        default:
            break;
    }
    // Обработчик должен быть максимально простым!
}

// Имитация "вычислений"
void do_work(int work_us) {
    // Просто "сжигаем" CPU в цикле
    long long end_time_us = 0;
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Переводим start.tv_sec и start.tv_nsec в микросекунды и добавляем work_us
    end_time_us = (long long)start.tv_sec * 1000000 + start.tv_nsec / 1000 + work_us;

    while (running) {
        // Текущее время в микросекундах
        clock_gettime(CLOCK_MONOTONIC, &current);
        long long current_time_us = (long long)current.tv_sec * 1000000 + current.tv_nsec / 1000;

        if (current_time_us >= end_time_us) {
            break;
        }
        // Небольшой счетчик, чтобы компилятор не оптимизировал цикл в ноль
        cycles_count++; 
    }
}

// Главная функция воркера
int run_worker(int worker_id, config_t *config) {
    pid_t pid = getpid();
    current_config = *config;
    current_mode = current_config.heavy; // Начинаем в тяжелом режиме
    
    // 1. Установка обработчиков сигналов
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask); // Не блокируем другие сигналы

    if (sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGINT, &sa, NULL) == -1 ||
        sigaction(SIGUSR1, &sa, NULL) == -1 ||
        sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("Worker: sigaction failed");
        return EXIT_FAILURE;
    }

    printf("[WORKER %d (PID %d)] Started. Initial mode: %s\n", worker_id, pid, mode_name);

    long long ticks = 0;

    // 2. Основной цикл работы
    while (running) {
        // Выполняем работу
        do_work(current_mode.work_us);

        // После работы проверяем, не нужно ли завершиться
        if (!running) break;

        // Пауза (имитация I/O или ожидания)
        usleep(current_mode.sleep_us);

        // Вывод статистики
        if (++ticks % 10 == 0) { // Каждые 10 тиков
             printf("[WORKER %d (PID %d)] Mode: %s. Tick: %lld. Cycles: %lld. Working on CPU: %d\n", 
                    worker_id, pid, mode_name, ticks, cycles_count, sched_getcpu());
        }
        
        // Сброс флага смены режима
        if (mode_changed) {
            printf("[WORKER %d (PID %d)] Mode switched to %s (Work %d us, Sleep %d us)\n", 
                   worker_id, pid, mode_name, current_mode.work_us, current_mode.sleep_us);
            mode_changed = 0;
        }
    }

    // 3. Корректное завершение
    printf("[WORKER %d (PID %d)] Graceful shutdown complete. Total cycles: %lld\n", worker_id, pid, cycles_count);
    return EXIT_SUCCESS;
}

// Точка входа для сборки как отдельного исполняемого файла (для отладки)
// В супервизоре будет использоваться напрямую `run_worker` через `fork/exec`
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <worker_id> [config_path]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    int worker_id = atoi(argv[1]);
    const char *config_path = (argc > 2) ? argv[2] : "config.ini";

    config_t cfg;
    if (read_config(config_path, &cfg) != 0) {
        fprintf(stderr, "Could not read config at %s. Using defaults.\n", config_path);
        set_default_config(&cfg);
    }
    
    return run_worker(worker_id, &cfg);
}