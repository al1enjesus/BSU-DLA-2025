#include "supervisor_shm.h"
#include <signal.h>
#include <unistd.h>
#include <iostream>

int main() {
    // Подпишемся на нужные сигналы
    signal(SIGTERM, supervisor_signal);
    signal(SIGINT, supervisor_signal);
    signal(SIGHUP, supervisor_signal);
    signal(SIGUSR1, supervisor_signal);
    signal(SIGUSR2, supervisor_signal);
    // SIGCHLD будем обрабатывать вручную через waitpid в цикле

    // Инициализация shared memory
    if(!setup_shared_memory("/app_shared_cfg")) return 1;

    // Инициализируем мьютекс для межпроцессного использования
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&g_shared->lock, &attr);

    // Загрузим конфиг и опубликуем его в shm
    LocalConfig cfg = load_config_file("config.json");
    if(cfg.workers <= 0) {
        std::cerr << "supervisor: no workers configured\n";
        // всё равно публикуем чтобы воркеры не упали при подключении
    }
    publish_config_to_shm(cfg);

    // Создаём воркеры
    if(cfg.workers > 0) {
        if(!spawn_workers_count(cfg.workers)) {
            shutdown_all_and_cleanup("/app_shared_cfg");
            return 2;
        }
    }

    std::cout << "[S] entering main loop pid=" << getpid() << "\n";

    // Основной цикл супервизора
    while(!g_terminate) {
        if(g_reload_cfg) {
            std::cout << "[S] reloading configuration\n";
            LocalConfig newcfg = load_config_file("config.json");
            publish_config_to_shm(newcfg);
            for(const auto &r : g_workers) if(r.pid > 0) kill(r.pid, SIGHUP);
            g_reload_cfg = 0;
        }

        if(g_switch_light_on) {
            std::cout << "[S] switching workers to LIGHT mode\n";
            for(const auto &r : g_workers) if(r.pid > 0) kill(r.pid, SIGUSR1);
            g_switch_light_on = 0;
        }

        if(g_switch_light_off) {
            std::cout << "[S] switching workers to HEAVY mode\n";
            for(const auto &r : g_workers) if(r.pid > 0) kill(r.pid, SIGUSR2);
            g_switch_light_off = 0;
        }

        // Проверяем завершившихся детей (WNOHANG)
        int status = 0;
        pid_t child = waitpid(-1, &status, WNOHANG);
        if(child > 0) {
            restart_if_needed(child);
        }

        usleep(200000);
    }

    shutdown_all_and_cleanup("/app_shared_cfg");
    return 0;
}

