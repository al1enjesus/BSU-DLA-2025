#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/resource.h>
#include "config.h"

volatile sig_atomic_t worker_running = 1;
volatile sig_atomic_t current_mode = 0;
config_t worker_config;
int worker_id = 0;

void signal_handler(int sig) {
    switch(sig) {
        case SIGTERM:
            worker_running = 0;
            printf("Worker %d: Received SIGTERM, shutting down...\n", getpid());
            break;
        case SIGUSR1:
            current_mode = 1;
            printf("Worker %d: Switching to LIGHT mode\n", getpid());
            break;
        case SIGUSR2:
            current_mode = 0;
            printf("Worker %d: Switching to HEAVY mode\n", getpid());
            break;
    }
}

void setup_signals() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    signal(SIGINT, SIG_IGN);
}

void simulate_work(int work_us, int sleep_us) {
    // Разбиваем работу на мелкие части для частой проверки флага
    const int chunk_us = 1000; // 1ms chunks
    int remaining_us = work_us;
    
    while (remaining_us > 0 && worker_running) {
        int chunk = (remaining_us > chunk_us) ? chunk_us : remaining_us;
        
        clock_t start = clock();
        while (clock() < start + (chunk * CLOCKS_PER_SEC / 1000000) && worker_running) {
            // busy wait с проверкой флага
        }
        
        remaining_us -= chunk;
        
        // Частая проверка флага завершения
        if (!worker_running) break;
    }
    
    // Сон тоже с проверкой флага
    if (worker_running) {
        remaining_us = sleep_us;
        while (remaining_us > 0 && worker_running) {
            int sleep_chunk = (remaining_us > 50000) ? 50000 : remaining_us; // 50ms max
            usleep(sleep_chunk);
            remaining_us -= sleep_chunk;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        worker_id = atoi(argv[1]);
    }
    
    printf("Worker[%d] %d started (parent: %d)\n", worker_id, getpid(), getppid());
    
    setup_signals();
    load_config(&worker_config);
    
    int tick_count = 0;
while (worker_running) {
    work_mode_t *mode = current_mode ? &worker_config.mode_light : &worker_config.mode_heavy;
    
    simulate_work(mode->work_us, mode->sleep_us);
    
    if (tick_count % 5 == 0) {
        print_worker_info();
    }
    
    tick_count++;
    
    // Дополнительная проверка после каждой итерации
    if (!worker_running) break;
}
    printf("Worker[%d] %d: Clean shutdown completed\n", worker_id, getpid());
    return 0;
}
