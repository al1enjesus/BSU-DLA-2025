#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <limits.h>
#include <sched.h>
#include <sys/resource.h>

#define MAX_WORKERS   256
#define MAX_RESTARTS  5
#define RESTART_WINDOW 30
#define RESTART_HISTORY 32

#define MAX_NICE_PAT 64
#define MAX_AFF_PAT 64

volatile sig_atomic_t sigchld_flag = 0;
volatile sig_atomic_t sigterm_flag = 0;
volatile sig_atomic_t sighup_flag = 0;
volatile sig_atomic_t sigusr1_flag = 0;
volatile sig_atomic_t sigusr2_flag = 0;

struct worker_entry {
    pid_t pid;
    int id;
    time_t restarts[RESTART_HISTORY];
    int restarts_count;
};

struct config {
    int workers;
    char initial_mode[16];
    int work_us_heavy;
    int sleep_us_heavy;
    int work_us_light;
    int sleep_us_light;
};

struct config cfg;
struct worker_entry workers[MAX_WORKERS];

/* patterns */
int nice_pat[MAX_NICE_PAT];
int nice_pat_len = 0;
cpu_set_t affinity_pat[MAX_AFF_PAT];
int affinity_pat_len = 0;

static void trim(char *s) {
    char *p = s;
    while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t L = strlen(s);
    while (L && (s[L-1] == ' ' || s[L-1] == '\t' || s[L-1] == '\r' || s[L-1] == '\n')) { s[L-1] = 0; L--; }
}

static void parse_nice_pattern(const char *v) {
    nice_pat_len = 0;
    char *tmp = strdup(v);
    char *save = NULL;
    char *tok = strtok_r(tmp, ",", &save);
    while (tok && nice_pat_len < MAX_NICE_PAT) {
        trim(tok);
        nice_pat[nice_pat_len++] = atoi(tok);
        tok = strtok_r(NULL, ",", &save);
    }
    free(tmp);
}

static void parse_affinity_pattern(const char *v) {
    affinity_pat_len = 0;
    char *tmp = strdup(v);
    char *save1 = NULL;
    char *block = strtok_r(tmp, "|", &save1);
    while (block && affinity_pat_len < MAX_AFF_PAT) {
        trim(block);
        CPU_ZERO(&affinity_pat[affinity_pat_len]);
        char *save2 = NULL;
        char *cpu_tok = strtok_r(block, ",", &save2);
        while (cpu_tok) {
            trim(cpu_tok);
            int c = atoi(cpu_tok);
            if (c >= 0 && c < CPU_SETSIZE) CPU_SET(c, &affinity_pat[affinity_pat_len]);
            cpu_tok = strtok_r(NULL, ",", &save2);
        }
        affinity_pat_len++;
        block = strtok_r(NULL, "|", &save1);
    }
    free(tmp);
}

void parse_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "open config: %s\n", strerror(errno));
        return;
    }

    /* defaults */
    cfg.workers = 2;
    strncpy(cfg.initial_mode, "heavy", sizeof(cfg.initial_mode) - 1);
    cfg.work_us_heavy = 9000;
    cfg.sleep_us_heavy = 1000;
    cfg.work_us_light = 2000;
    cfg.sleep_us_light = 8000;
    nice_pat_len = 0;
    affinity_pat_len = 0;

    char line[512];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || strlen(line) < 3) continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = line;
        char *v = eq + 1;
        trim(k);
        trim(v);
        if (strcmp(k, "workers") == 0) cfg.workers = atoi(v);
        else if (strcmp(k, "initial_mode") == 0) strncpy(cfg.initial_mode, v, sizeof(cfg.initial_mode)-1);
        else if (strcmp(k, "work_us_heavy") == 0) cfg.work_us_heavy = atoi(v);
        else if (strcmp(k, "sleep_us_heavy") == 0) cfg.sleep_us_heavy = atoi(v);
        else if (strcmp(k, "work_us_light") == 0) cfg.work_us_light = atoi(v);
        else if (strcmp(k, "sleep_us_light") == 0) cfg.sleep_us_light = atoi(v);
        else if (strcmp(k, "nice_pattern") == 0) parse_nice_pattern(v);
        else if (strcmp(k, "affinity_pattern") == 0) parse_affinity_pattern(v);
    }
    fclose(f);

    printf("[SUP] config loaded: workers=%d initial_mode=%s\n", cfg.workers, cfg.initial_mode);
    if (nice_pat_len) {
        printf("[SUP] nice pattern:");
        for (int i = 0; i < nice_pat_len; ++i) printf(" %d", nice_pat[i]);
        printf("\n");
    }
    if (affinity_pat_len) {
        printf("[SUP] affinity pattern parsed, blocks=%d\n", affinity_pat_len);
    }
}

void sigchld_handler(int s) { sigchld_flag = 1; }
void sigterm_handler(int s) { sigterm_flag = 1; }
void sighup_handler(int s) { sighup_flag = 1; }
void sigusr1_handler(int s) { sigusr1_flag = 1; }
void sigusr2_handler(int s) { sigusr2_flag = 1; }

void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sighup_handler;
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &sa, NULL);
}

int should_restart(int wid) {
    time_t now = time(NULL);
    int i, j = 0;
    for (i = 0; i < workers[wid].restarts_count; ++i) {
        if (now - workers[wid].restarts[i] <= RESTART_WINDOW) {
            workers[wid].restarts[j++] = workers[wid].restarts[i];
        }
    }
    workers[wid].restarts_count = j;
    if (workers[wid].restarts_count >= MAX_RESTARTS) return 0;
    if (workers[wid].restarts_count < RESTART_HISTORY) {
        workers[wid].restarts[workers[wid].restarts_count++] = now;
    }
    return 1;
}

void start_worker(int wid, const char *config_path) {
    pid_t pid = fork();
    if (pid == 0) {
        char idbuf[16];
        char cfgbuf[PATH_MAX];
        char nicebuf[16];
        char affinitybuf[256];

        snprintf(idbuf, sizeof idbuf, "%d", wid);
        snprintf(cfgbuf, sizeof cfgbuf, "%s", config_path);

        if (nice_pat_len > 0) {
            int n = nice_pat[ wid % nice_pat_len ];
            snprintf(nicebuf, sizeof nicebuf, "%d", n);
        } else {
            snprintf(nicebuf, sizeof nicebuf, "%d", 0);
        }

        if (affinity_pat_len > 0) {
            cpu_set_t *mask = &affinity_pat[ wid % affinity_pat_len ];
            int first = 1;
            affinitybuf[0] = 0;
            for (int c = 0; c < CPU_SETSIZE; ++c) {
                if (CPU_ISSET(c, mask)) {
                    char tmp[8];
                    if (!first) strncat(affinitybuf, ",", sizeof affinitybuf - strlen(affinitybuf) - 1);
                    snprintf(tmp, sizeof tmp, "%d", c);
                    strncat(affinitybuf, tmp, sizeof affinitybuf - strlen(affinitybuf) - 1);
                    first = 0;
                }
            }
            if (first) affinitybuf[0] = 0;
        } else {
            affinitybuf[0] = 0;
        }

        /* debug print */
        if (affinitybuf[0] != 0) {
            fprintf(stderr, "[SUP-debug] exec args: ./worker --id %s --config %s --nice %s --affinity %s\n",
                    idbuf, cfgbuf, nicebuf, affinitybuf);
            execl("./worker", "./worker",
                  "--id", idbuf,
                  "--config", cfgbuf,
                  "--nice", nicebuf,
                  "--affinity", affinitybuf,
                  (char*)NULL);
        } else {
            fprintf(stderr, "[SUP-debug] exec args: ./worker --id %s --config %s --nice %s\n",
                    idbuf, cfgbuf, nicebuf);
            execl("./worker", "./worker",
                  "--id", idbuf,
                  "--config", cfgbuf,
                  "--nice", nicebuf,
                  (char*)NULL);
        }
        perror("execl worker");
        _exit(1);
    } else if (pid > 0) {
        workers[wid].pid = pid;
        workers[wid].id = wid;
        printf("[SUP] started worker id=%d pid=%d\n", wid, pid);
    } else {
        perror("fork");
    }
}

void terminate_all_children(int sig) {
    for (int i = 0; i < cfg.workers; ++i) {
        if (workers[i].pid > 0) {
            kill(workers[i].pid, sig);
            printf("[SUP] sent signal %d to pid=%d\n", sig, workers[i].pid);
        }
    }
}

void reap_children_and_maybe_restart(const char *config_path) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        int wid = -1;
        for (int i = 0; i < cfg.workers; ++i) {
            if (workers[i].pid == pid) { wid = i; break; }
        }
        int exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        printf("[SUP] reaped pid=%d exit=%d\n", pid, exitcode);
        if (wid >= 0) {
            workers[wid].pid = 0;
            if (!sigterm_flag) {
                if (should_restart(wid)) {
                    printf("[SUP] restarting worker id=%d\n", wid);
                    start_worker(wid, config_path);
                } else {
                    printf("[SUP] worker id=%d exceeded restart limit -> not restarting\n", wid);
                }
            }
        } else {
            printf("[SUP] unknown child pid=%d reaped\n", pid);
        }
    }
}

int main(int argc, char **argv) {
    char config_path_buf[PATH_MAX] = {0};
    const char *config_path = NULL;

    if (argc >= 2 && argv[1][0] != '\0') {
        config_path = argv[1];
    } else {
        char exe_real[PATH_MAX];
        if (realpath(argv[0], exe_real) != NULL) {
            char *slash = strrchr(exe_real, '/');
            if (slash) {
                size_t dirlen = slash - exe_real;
                if (dirlen + strlen("/config.cfg") < sizeof(config_path_buf)) {
                    snprintf(config_path_buf, sizeof(config_path_buf), "%.*s/config.cfg", (int)dirlen, exe_real);
                    config_path = config_path_buf;
                }
            }
        }
        if (config_path == NULL) config_path = "config.cfg";
    }

    parse_config(config_path);
    setup_signal_handlers();

    for (int i = 0; i < MAX_WORKERS; i++) { workers[i].pid = 0; workers[i].restarts_count = 0; }

    for (int i = 0; i < cfg.workers; ++i) start_worker(i, config_path);

    printf("[SUP] monitor loop enter\n");

    while (1) {
        if (sigchld_flag) {
            sigchld_flag = 0;
            reap_children_and_maybe_restart(config_path);
        }
        if (sighup_flag) {
            sighup_flag = 0;
            printf("[SUP] SIGHUP received -> reload config and replace workers\n");
            parse_config(config_path);
            for (int i = 0; i < cfg.workers; ++i) {
                pid_t old = workers[i].pid;
                printf("[SUP] starting replacement for worker %d\n", i);
                start_worker(i, config_path);
                if (old > 0) {
                    kill(old, SIGTERM);
                    printf("[SUP] terminated old worker pid=%d\n", old);
                }
            }
        }
        if (sigusr1_flag) {
            sigusr1_flag = 0;
            printf("[SUP] SIGUSR1 -> broadcasting to workers\n");
            for (int i = 0; i < cfg.workers; ++i) if (workers[i].pid > 0) kill(workers[i].pid, SIGUSR1);
        }
        if (sigusr2_flag) {
            sigusr2_flag = 0;
            printf("[SUP] SIGUSR2 -> broadcasting to workers\n");
            for (int i = 0; i < cfg.workers; ++i) if (workers[i].pid > 0) kill(workers[i].pid, SIGUSR2);
        }
        if (sigterm_flag) {
            printf("[SUP] SIGTERM/SIGINT received -> graceful shutdown\n");
            terminate_all_children(SIGTERM);
            sleep(1);
            for (int waitt = 0; waitt < 10; ++waitt) {
                int alive = 0;
                for (int i = 0; i < cfg.workers; ++i) if (workers[i].pid > 0) alive++;
                if (!alive) break;
                sleep(1);
            }
            terminate_all_children(SIGKILL);
            printf("[SUP] shutdown complete\n");
            break;
        }
        usleep(200000);
    }

    return 0;
}
