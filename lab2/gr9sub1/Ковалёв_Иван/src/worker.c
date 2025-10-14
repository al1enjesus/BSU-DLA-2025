#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sched.h>
#include <sys/resource.h>

volatile sig_atomic_t shutdown_flag = 0;
volatile sig_atomic_t reload_flag = 0;
volatile sig_atomic_t switch_light = 0;
volatile sig_atomic_t switch_heavy = 0;

struct config {
    char initial_mode[16];
    int work_us_heavy;
    int sleep_us_heavy;
    int work_us_light;
    int sleep_us_light;
};

struct config cfg;

static void trim(char *s) {
    char *p = s;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t L = strlen(s);
    while (L && (s[L-1] == ' ' || s[L-1] == '\t' || s[L-1] == '\r' || s[L-1] == '\n')) { s[L-1] = 0; L--; }
}

void parse_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("open config");
        return;
    }
    strcpy(cfg.initial_mode, "heavy");
    cfg.work_us_heavy = 9000;
    cfg.sleep_us_heavy = 1000;
    cfg.work_us_light = 2000;
    cfg.sleep_us_light = 8000;

    char line[512];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || strlen(line) < 3) continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = line;
        char *v = eq + 1;
        trim(k); trim(v);
        if (strcmp(k, "initial_mode") == 0) strncpy(cfg.initial_mode, v, sizeof(cfg.initial_mode) - 1);
        else if (strcmp(k, "work_us_heavy") == 0) cfg.work_us_heavy = atoi(v);
        else if (strcmp(k, "sleep_us_heavy") == 0) cfg.sleep_us_heavy = atoi(v);
        else if (strcmp(k, "work_us_light") == 0) cfg.work_us_light = atoi(v);
        else if (strcmp(k, "sleep_us_light") == 0) cfg.sleep_us_light = atoi(v);
    }
    fclose(f);
    printf("[WORK %d] config loaded\n", getpid());
}

void term_handler(int s) { shutdown_flag = 1; }
void hup_handler(int s) { reload_flag = 1; }
void usr1_handler(int s) { switch_light = 1; }
void usr2_handler(int s) { switch_heavy = 1; }

void busy_work_microsec(int microsec) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    double target = microsec / 1e6;
    volatile uint64_t x = 1;
    while (1) {
        x = x * 11400714819323198485ull + 1;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double diff = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        if (diff >= target) break;
    }
}

static void print_affinity_summary() {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (sched_getaffinity(0, sizeof(mask), &mask) == 0) {
        int first = 1;
        printf("[WORK %d] affinity:", getpid());
        for (int c = 0; c < CPU_SETSIZE; ++c) {
            if (CPU_ISSET(c, &mask)) {
                if (!first) printf(",");
                printf("%d", c);
                first = 0;
            }
        }
        if (first) printf(" none");
        printf("\n");
    } else {
        printf("[WORK %d] affinity: sched_getaffinity failed: %s\n", getpid(), strerror(errno));
    }
}

int main(int argc, char **argv) {
    int id = 0;
    const char *config_path = "config.cfg";
    int supplied_nice = 0;
    int have_nice = 0;
    char affinity_str[256] = "";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--id") == 0 && i + 1 < argc) { id = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) { config_path = argv[++i]; }
        else if (strcmp(argv[i], "--nice") == 0 && i + 1 < argc) { supplied_nice = atoi(argv[++i]); have_nice = 1; }
        else if (strcmp(argv[i], "--affinity") == 0 && i + 1 < argc) { strncpy(affinity_str, argv[++i], sizeof(affinity_str) - 1); }
    }

    /* Apply nice if provided */
    if (have_nice) {
        if (setpriority(PRIO_PROCESS, 0, supplied_nice) != 0) {
            fprintf(stderr, "[WORK %d] setpriority(%d) failed: %s\n", getpid(), supplied_nice, strerror(errno));
        } else {
            printf("[WORK %d] set nice=%d (applied)\n", getpid(), supplied_nice);
        }
    } else {
        int cur = getpriority(PRIO_PROCESS, 0);
        if (!(cur == -1 && errno)) printf("[WORK %d] current nice=%d\n", getpid(), cur);
    }

    /* Apply affinity if provided */
    if (affinity_str[0] != '\0') {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        char *tmp = strdup(affinity_str);
        char *save = NULL;
        char *tok = strtok_r(tmp, ",", &save);
        while (tok) {
            int cpu = atoi(tok);
            if (cpu >= 0 && cpu < CPU_SETSIZE) CPU_SET(cpu, &mask);
            tok = strtok_r(NULL, ",", &save);
        }
        free(tmp);
        if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
            fprintf(stderr, "[WORK %d] sched_setaffinity(%s) failed: %s\n", getpid(), affinity_str, strerror(errno));
        } else {
            printf("[WORK %d] applied affinity=%s\n", getpid(), affinity_str);
        }
    } else {
        print_affinity_summary();
    }

    /* Install signal handlers */
    struct sigaction sa;
    sa.sa_handler = term_handler; sigemptyset(&sa.sa_mask); sa.sa_flags = 0; sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = hup_handler; sigaction(SIGHUP, &sa, NULL);
    sa.sa_handler = usr1_handler; sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = usr2_handler; sigaction(SIGUSR2, &sa, NULL);

    /* load config and start loop */
    parse_config(config_path);
    char mode[16];
    strncpy(mode, cfg.initial_mode, sizeof(mode) - 1);

    int ticks = 0;
    while (!shutdown_flag) {
        if (reload_flag) {
            reload_flag = 0;
            parse_config(config_path);
        }
        if (switch_light) { switch_light = 0; strncpy(mode, "light", sizeof(mode) - 1); printf("[WORK %d] switched to LIGHT\n", getpid()); }
        if (switch_heavy) { switch_heavy = 0; strncpy(mode, "heavy", sizeof(mode) - 1); printf("[WORK %d] switched to HEAVY\n", getpid()); }

        int work_us = (strcmp(mode, "heavy") == 0) ? cfg.work_us_heavy : cfg.work_us_light;
        int sleep_us = (strcmp(mode, "heavy") == 0) ? cfg.sleep_us_heavy : cfg.sleep_us_light;
        busy_work_microsec(work_us);
        ticks++;
        if (ticks % 5 == 0) {
            int pr2 = getpriority(PRIO_PROCESS, 0);
            printf("[WORK %d] mode=%s tick=%d pid=%d nice=%d ", getpid(), mode, ticks, getpid(), pr2);
            print_affinity_summary();
        }
        struct timespec req = { .tv_sec = 0, .tv_nsec = (long)sleep_us * 1000L };
        nanosleep(&req, NULL);
    }

    printf("[WORK %d] graceful exit (ticks=%d)\n", getpid(), ticks);
    return 0;
}
