#include "config.h"
#include <stdio.h>

void load_config(config_t *config) {
    config->workers_count = DEFAULT_WORKERS;
    config->mode_heavy.work_us = 500000;  // 500ms work
    config->mode_heavy.sleep_us = 100000; // 100ms sleep
    config->mode_light.work_us = 100000;  // 100ms work  
    config->mode_light.sleep_us = 500000; // 500ms sleep
}

void reload_config(config_t *config) {
    printf("Reloading configuration...\n");
    load_config(config);
}
