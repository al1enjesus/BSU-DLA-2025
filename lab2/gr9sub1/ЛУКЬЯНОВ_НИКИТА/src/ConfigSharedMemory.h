#ifndef CONFIG_SHARED_MEMORY_H
#define CONFIG_SHARED_MEMORY_H

#include <pthread.h>
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

struct WorkerConfig {
    pthread_mutex_t mtx;
    int heavyWorkUs;
    int heavySleepUs;
    int lightWorkUs;
    int lightSleepUs;
};

struct ConfigData {
    int workers;
    int heavyWorkUs;
    int heavySleepUs;
    int lightWorkUs;
    int lightSleepUs;
};

static ConfigData readConfigFile() {
    std::ifstream cfgFile("config.json");
    
    if(!cfgFile.is_open()) {
        std::cout << "Failed to open config file\n";
        return ConfigData{0, 0, 0, 0, 0};;
    }

    json cfgJson;

    cfgFile >> cfgJson;

    return ConfigData { 
        (int)cfgJson["workers"], 
        (int)cfgJson["mode_heavy"]["work_us"], 
        (int)cfgJson["mode_heavy"]["sleep_us"],
        (int)cfgJson["mode_light"]["work_us"],
        (int)cfgJson["mode_light"]["sleep_us"] };  
}

#endif //CONFIG_SHARED_MEMORY_H