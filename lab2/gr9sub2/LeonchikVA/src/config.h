#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int work_us; 
    int sleep_us;
} mode_params_t;

typedef struct {
   int workers;
    mode_params_t heavy;
    mode_params_t light;
    char config_path[256]; 
    
    // часть B
    int nice_default; 
    int nice_low_prio;
    int affinity_cpu0;
    int affinity_cpu1
} config_t;

int read_config(const char* path, config_t* config);

void set_default_config(config_t* config);

#endif // CONFIG_H