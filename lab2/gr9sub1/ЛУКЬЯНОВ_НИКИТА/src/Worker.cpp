#include <iostream>
#include <cmath>
#include <csignal>
#include <unistd.h>
#include <sched.h>
#include <chrono>
#include <sys/mman.h>
#include <fcntl.h>

#include "ConfigSharedMemory.h"

volatile sig_atomic_t running = 1;
volatile sig_atomic_t lightMode = 0;
volatile sig_atomic_t reload = 1;

void handle_signal(int sig) {
    if(sig == SIGTERM || sig == SIGINT) {
        std::cout << getpid() << " Get SIGTERM\n";
        running = 0;
    }
    if(sig == SIGUSR1) lightMode = 1;
    if(sig == SIGUSR2) lightMode = 0;
    if(sig == SIGHUP) reload = 1;
}

void heavy_work(int work_us) {
    auto start = std::chrono::steady_clock::now();
    double dummy = 0;
    while(true) {
        dummy += std::sin(0.1) * std::cos(0.2);
        auto now = std::chrono::steady_clock::now();
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
        if(elapsed_us >= work_us) break;
    }
}


int main() {
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);
    signal(SIGHUP, handle_signal);

    int fd = shm_open("/shared_worker_config", O_RDWR, 0666);
    if(fd == -1) { perror("shm_open"); return 1; }

    WorkerConfig* cfg = (WorkerConfig*) mmap(nullptr, sizeof(WorkerConfig),
                                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(cfg == MAP_FAILED) { perror("mmap"); return 1; }

    pid_t pid = getpid();
    uint64_t tick = 0;

    ConfigData cfgData{0, 0, 0, 0};
    while(running) {
        int cpu = sched_getcpu();
        if(reload) {
            pthread_mutex_lock(&cfg->mtx);

            cfgData = {
                0,
                cfg->heavyWorkUs, 
                cfg->heavySleepUs,
                cfg->lightWorkUs,
                cfg->lightSleepUs
            };

            std::cout << "[Worker " << pid << "] Tick " << tick
                   << " Mode: " << (lightMode ? "LIGHT" : "HEAVY")
                   << " CPU: " << cpu
                   << " Heavy mode status: " << "Work: " << cfgData.heavyWorkUs << " Sleep: " << cfgData.heavySleepUs
                   << " Light mode status: " << "Work: " << cfgData.lightWorkUs << " Sleep: " << cfgData.lightSleepUs
                   << std::endl;

            pthread_mutex_unlock(&cfg->mtx);
            reload = 0;
        }

        int work_us = lightMode ? cfgData.lightWorkUs : cfgData.heavyWorkUs;
        int sleep_us = lightMode ? cfgData.lightSleepUs : cfgData.heavySleepUs;

        heavy_work(work_us);
        tick++;

        usleep(sleep_us);
    }

    std::cout << "[Worker " << pid << "] Exiting gracefully\n";
    return 0;
}