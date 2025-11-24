#include "config.h"
#include <unistd.h>

// Функция для парсинга "KEY=VALUE"
static int parse_line(const char *line, config_t *config) {
    char key[64], value[64];
    int res = sscanf(line, "%63[^=]=%63s", key, value);

    if (res != 2) return -1; // Некорректный формат

    // Парсинг ключей
    if (strcmp(key, "workers") == 0) {
        config->workers = atoi(value);
    } else if (strcmp(key, "work_heavy_us") == 0) {
        config->heavy.work_us = atoi(value);
    } else if (strcmp(key, "sleep_heavy_us") == 0) {
        config->heavy.sleep_us = atoi(value);
    } else if (strcmp(key, "work_light_us") == 0) {
        config->light.work_us = atoi(value);
    } else if (strcmp(key, "sleep_light_us") == 0) {
        config->light.sleep_us = atoi(value);
    // --- НОВЫЙ КОД ---
    } else if (strcmp(key, "nice_default") == 0) {
        config->nice_default = atoi(value);
    } else if (strcmp(key, "nice_low_prio") == 0) {
        config->nice_low_prio = atoi(value);
    } else if (strcmp(key, "affinity_cpu0") == 0) {
        config->affinity_cpu0 = atoi(value);
    } else if (strcmp(key, "affinity_cpu1") == 0) {
        config->affinity_cpu1 = atoi(value);
    // --- КОНЕЦ НОВОГО КОДА ---
    } else {
        return -2; // Неизвестный ключ
    }
    return 0;
}

void set_default_config(config_t* config) {
    config->workers = 2;
    config->heavy.work_us = 9000;
    config->heavy.sleep_us = 1000;
    config->light.work_us = 2000;
    config->light.sleep_us = 8000;
    strcpy(config->config_path, "config.ini");
    
    // --- НОВЫЕ ДЕФОЛТЫ ---
    config->nice_default = 0;
    config->nice_low_prio = 10;
    config->affinity_cpu0 = 0;
    config->affinity_cpu1 = 1;
    // --- КОНЕЦ НОВЫХ ДЕФОЛТОВ ---
}

// Добавить перед read_config
static int validate_config(config_t *config) {
    // Проверка количества воркеров
    if (config->workers < 1 || config->workers > 100) {
        fprintf(stderr, "Invalid workers count: %d (must be 1-100)\n", config->workers);
        return -1;
    }
    
    // Проверка таймингов
    if (config->heavy.work_us < 0 || config->heavy.work_us > 1000000) {
        fprintf(stderr, "Invalid work_heavy_us: %d (must be 0-1000000)\n", config->heavy.work_us);
        return -1;
    }
    if (config->heavy.sleep_us < 0 || config->heavy.sleep_us > 1000000) {
        fprintf(stderr, "Invalid sleep_heavy_us: %d\n", config->heavy.sleep_us);
        return -1;
    }
    if (config->light.work_us < 0 || config->light.work_us > 1000000) {
        fprintf(stderr, "Invalid work_light_us: %d\n", config->light.work_us);
        return -1;
    }
    if (config->light.sleep_us < 0 || config->light.sleep_us > 1000000) {
        fprintf(stderr, "Invalid sleep_light_us: %d\n", config->light.sleep_us);
        return -1;
    }
    
    // Проверка nice значений
    if (config->nice_default < -20 || config->nice_default > 19) {
        fprintf(stderr, "Invalid nice_default: %d (must be -20 to 19)\n", config->nice_default);
        return -1;
    }
    if (config->nice_low_prio < -20 || config->nice_low_prio > 19) {
        fprintf(stderr, "Invalid nice_low_prio: %d (must be -20 to 19)\n", config->nice_low_prio);
        return -1;
    }
    
    // Проверка CPU affinity (получаем количество CPU в системе)
    long num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cpus <= 0) {
        fprintf(stderr, "Warning: Could not determine CPU count\n");
        num_cpus = 2; // fallback
    }
    
    if (config->affinity_cpu0 < 0 || config->affinity_cpu0 >= num_cpus) {
        fprintf(stderr, "Invalid affinity_cpu0: %d (system has %ld CPUs)\n", 
                config->affinity_cpu0, num_cpus);
        return -1;
    }
    if (config->affinity_cpu1 < 0 || config->affinity_cpu1 >= num_cpus) {
        fprintf(stderr, "Invalid affinity_cpu1: %d (system has %ld CPUs)\n", 
                config->affinity_cpu1, num_cpus);
        return -1;
    }
    
    return 0;
}

// Изменить функцию read_config - добавить вызов валидации в конце:
int read_config(const char* path, config_t* config) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("Error opening config file");
        return -1;
    }
    
    set_default_config(config);
    strcpy(config->config_path, path);
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char *p = line;
        while(*p && *p != '\n' && *p != '#') p++;
        *p = '\0';
        if (strlen(line) > 0 && line[0] != '#') {
            parse_line(line, config);
        }
    }
    fclose(file);
    
    // ДОБАВИТЬ ВАЛИДАЦИЮ
    if (validate_config(config) != 0) {
        fprintf(stderr, "Config validation failed for %s\n", path);
        return -1;
    }
    
    return 0;
}