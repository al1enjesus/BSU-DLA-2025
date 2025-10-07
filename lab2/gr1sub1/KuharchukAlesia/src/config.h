#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_WORKERS 3

typedef struct {
    int work_us;
    int sleep_us;
} work_mode_t;

typedef struct {
    int workers_count;
    work_mode_t mode_heavy;
    work_mode_t mode_light;
} config_t;

void load_config(config_t *config);
void reload_config(config_t *config);
void print_config(const config_t *config);

#endif
