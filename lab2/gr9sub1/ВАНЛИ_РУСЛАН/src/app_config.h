#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <pthread.h>
#include <fstream>
#include <iostream>
#include "json.hpp"

using json = nlohmann::json;

// Структура, размещаемая в разделяемой памяти
struct SharedConfig {
    pthread_mutex_t lock;
    int heavy_work_us;
    int heavy_sleep_us;
    int light_work_us;
    int light_sleep_us;
};

// Локальная структура для чтения/кеширования конфига
struct LocalConfig {
    int workers;
    int heavy_work_us;
    int heavy_sleep_us;
    int light_work_us;
    int light_sleep_us;
};

// Читает config.json и возвращает LocalConfig.
// Функция inline чтобы её можно было определить в хэдере.
inline LocalConfig load_config_file(const char* filename = "config.json") {
    std::ifstream in(filename);
    if(!in.is_open()) {
        std::cerr << "load_config_file: cannot open " << filename << "\n";
        return {0,0,0,0,0};
    }

    json j;
    in >> j;

    LocalConfig cfg;
    cfg.workers = j.value("workers", 0);
    cfg.heavy_work_us = j["mode_heavy"].value("work_us", 0);
    cfg.heavy_sleep_us = j["mode_heavy"].value("sleep_us", 0);
    cfg.light_work_us = j["mode_light"].value("work_us", 0);
    cfg.light_sleep_us = j["mode_light"].value("sleep_us", 0);

    return cfg;
}

#endif // APP_CONFIG_H

