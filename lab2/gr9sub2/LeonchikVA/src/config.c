#include "config.h"

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

int read_config(const char* path, config_t* config) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        perror("Error opening config file");
        return -1;
    }

    // Устанавливаем дефолты перед чтением
    set_default_config(config);
    strcpy(config->config_path, path);

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Удаляем перевод строки и пробелы
        char *p = line;
        while(*p && *p != '\n' && *p != '#') p++;
        *p = '\0';

        if (strlen(line) > 0 && line[0] != '#') {
            parse_line(line, config);
        }
    }

    fclose(file);
    return 0;
}