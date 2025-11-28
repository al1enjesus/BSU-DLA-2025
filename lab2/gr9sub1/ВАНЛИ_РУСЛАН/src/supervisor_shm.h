#ifndef SUPERVISOR_SHM_H
#define SUPERVISOR_SHM_H

#include "app_config.h"

#include <vector>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <chrono>
#include <pthread.h>
#include <iostream>

// Инструменты для управления рабочими процессами
struct WorkerRecord {
    pid_t pid;
    std::chrono::steady_clock::time_point last_spawn;
    int restart_attempts;
};

static SharedConfig* g_shared = nullptr;
static std::vector<WorkerRecord> g_workers;

// Флаги, изменяемые сигналами
static volatile sig_atomic_t g_terminate = 0;
static volatile sig_atomic_t g_reload_cfg = 0;
static volatile sig_atomic_t g_switch_light_on = 0;
static volatile sig_atomic_t g_switch_light_off = 0;

// Обработчик сигналов (у супервизора)
static void supervisor_signal(int sig) {
    if(sig == SIGTERM || sig == SIGINT) g_terminate = 1;
    else if(sig == SIGHUP) g_reload_cfg = 1;
    else if(sig == SIGUSR1) g_switch_light_on = 1;
    else if(sig == SIGUSR2) g_switch_light_off = 1;
    else if(sig == SIGCHLD) {
        // SIGCHLD будет обрабатываться через waitpid в основном цикле
    }
}

// Создаём или подключаем shared memory (имя должно совпадать с worker)
inline bool setup_shared_memory(const char* name = "/app_shared_cfg") {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if(fd == -1) {
        std::cerr << "supervisor: shm_open failed\n";
        return false;
    }
    if(ftruncate(fd, sizeof(SharedConfig)) == -1) {
        perror("supervisor: ftruncate");
        return false;
    }
    g_shared = (SharedConfig*) mmap(nullptr, sizeof(SharedConfig),
                                    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(g_shared == MAP_FAILED) {
        perror("supervisor: mmap");
        return false;
    }
    return true;
}

// Заполняем shared memory начальными значениями (взяв из LocalConfig)
inline void publish_config_to_shm(const LocalConfig& cfg) {
    pthread_mutex_lock(&g_shared->lock);

    g_shared->heavy_work_us  = cfg.heavy_work_us;
    g_shared->heavy_sleep_us = cfg.heavy_sleep_us;
    g_shared->light_work_us  = cfg.light_work_us;
    g_shared->light_sleep_us = cfg.light_sleep_us;

    pthread_mutex_unlock(&g_shared->lock);
}

// Спавним одного воркера
inline bool spawn_worker(int index, int restart_count = 0) {
    pid_t p = fork();
    if(p < 0) {
        perror("spawn_worker: fork");
        return false;
    } else if(p == 0) {
        // дочерний: запускаем бинарь worker (ожидается ./worker)
        execl("./worker", "./worker", nullptr);
        perror("spawn_worker: execl");
        _exit(127);
    } else {
        // родитель
        g_workers[index].pid = p;
        g_workers[index].last_spawn = std::chrono::steady_clock::now();
        g_workers[index].restart_attempts = restart_count;
        std::cout << "[S] spawned pid=" << p << "\n";
        return true;
    }
}

// Создаём N воркеров
inline bool spawn_workers_count(int n) {
    if(!g_shared) {
        std::cerr << "spawn_workers_count: shared not initialized\n";
        return false;
    }
    g_workers.clear();
    g_workers.resize(n);
    for(int i=0;i<n;++i) {
        if(!spawn_worker(i, 0)) return false;
    }
    return true;
}

// Обработка завершения воркера: найти индекс и перезапустить (с ограничением)
inline void restart_if_needed(pid_t dead_pid) {
    std::cout << "[S] worker pid=" << dead_pid << " terminated; attempting restart\n";
    size_t idx = 0;
    for(; idx < g_workers.size(); ++idx) {
        if(g_workers[idx].pid == dead_pid) break;
    }
    if(idx == g_workers.size()) {
        std::cerr << "[S] unknown child pid=" << dead_pid << "\n";
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_workers[idx].last_spawn).count();
    if(elapsed > 30) g_workers[idx].restart_attempts = 0;

    if(g_workers[idx].restart_attempts < 5) {
        ++g_workers[idx].restart_attempts;
        spawn_worker(idx, g_workers[idx].restart_attempts);
    } else {
        std::cerr << "[S] too many restarts for pid=" << dead_pid << "\n";
    }
}

// Попытаться корректно завершить всех воркеров и очистить ресурсы
inline void shutdown_all_and_cleanup(const char* shmname = "/app_shared_cfg") {
    std::cout << "[S] shutting down workers...\n";
    for(auto &r : g_workers) {
        if(r.pid > 0) {
            kill(r.pid, SIGTERM);
        }
    }
    // дождёмся детей
    for(size_t i = 0; i < g_workers.size(); ++i) wait(nullptr);

    pthread_mutex_destroy(&g_shared->lock);
    shm_unlink(shmname);
    std::cout << "[S] cleanup done\n";
}

#endif // SUPERVISOR_SHM_H

