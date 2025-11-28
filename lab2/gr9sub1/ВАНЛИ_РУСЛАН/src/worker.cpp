#include "app_config.h"

#include <iostream>
#include <cmath>
#include <csignal>
#include <unistd.h>
#include <sched.h>
#include <chrono>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>

static volatile sig_atomic_t s_keep_running = 1;
static volatile sig_atomic_t s_light_mode = 0;
static volatile sig_atomic_t s_need_reload = 1;

static void signal_handler(int sig) {
    switch(sig) {
        case SIGTERM: case SIGINT:
            s_keep_running = 0;
            break;
        case SIGUSR1:
            s_light_mode = 1;
            break;
        case SIGUSR2:
            s_light_mode = 0;
            break;
        case SIGHUP:
            s_need_reload = 1;
            break;
        default:
            break;
    }
}

static void busy_work_for_us(int usec) {
    auto start = std::chrono::steady_clock::now();
    volatile unsigned long long a = 0, b = 1;
    while (true) {
        unsigned long long next = a + b;
        a = b;
        b = next;

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::microseconds>(now - start).count() >= usec)
            break;
    }
}

int main() {
    // Настроим обработчики сигналов
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    signal(SIGHUP, signal_handler);

    // Откроем разделяемую память (должен совпадать с именем супервизора)
    const char* shm_name = "/app_shared_cfg";
    int fd = shm_open(shm_name, O_RDWR, 0);
    if(fd == -1) { perror("worker: shm_open"); return 2; }

    SharedConfig* shared = (SharedConfig*) mmap(nullptr, sizeof(SharedConfig),
                                                PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shared == MAP_FAILED) { perror("worker: mmap"); return 3; }

    pid_t me = getpid();
    uint64_t tick = 0;
    LocalConfig local = {0,0,0,0,0};

    while(s_keep_running) {
        int cpu = sched_getcpu();

        if(s_need_reload) {
            pthread_mutex_lock(&shared->lock);

            // Читаем значения из разделяемой памяти в локальную структуру
            local.heavy_work_us  = shared->heavy_work_us;
            local.heavy_sleep_us = shared->heavy_sleep_us;
            local.light_work_us  = shared->light_work_us;
            local.light_sleep_us = shared->light_sleep_us;

            pthread_mutex_unlock(&shared->lock);

            std::cout << "[W " << me << "] tick=" << tick
                      << " mode=" << (s_light_mode ? "LIGHT" : "HEAVY")
                      << " cpu=" << cpu
                      << " heavy(work/sleep)=" << local.heavy_work_us << "/" << local.heavy_sleep_us
                      << " light(work/sleep)=" << local.light_work_us << "/" << local.light_sleep_us
                      << std::endl;

            s_need_reload = 0;
        }

        int usec_work = s_light_mode ? local.light_work_us : local.heavy_work_us;
        int usec_sleep = s_light_mode ? local.light_sleep_us : local.heavy_sleep_us;

        // Выполняем нагрузку и спим
        if(usec_work > 0) busy_work_for_us(usec_work);
        ++tick;
        if(usec_sleep > 0) usleep(usec_sleep);
    }

    std::cout << "[W " << me << "] shutting down\n";
    return 0;
}

