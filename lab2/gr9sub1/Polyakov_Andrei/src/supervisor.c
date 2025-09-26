#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <sys/resource.h>
#include <sched.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_WORKERS 64
#define RESTART_HISTORY 32

volatile sig_atomic_t got_sigchld = 0;
volatile sig_atomic_t got_term = 0;
volatile sig_atomic_t got_hup = 0;
volatile sig_atomic_t got_usr1 = 0;
volatile sig_atomic_t got_usr2 = 0;

typedef struct {
    pid_t pid;
    int slot;
    int active;
    time_t restarts[RESTART_HISTORY];
    int restart_idx;
} worker_info_t;

typedef struct {
    int workers;
    int heavy_work_us;
    int heavy_sleep_us;
    int light_work_us;
    int light_sleep_us;
    int restart_count;
    int restart_window_s;
    int nice_high;
    int nice_low;
} cfg_t;

worker_info_t workers[MAX_WORKERS];
cfg_t cfg_default = {
    .workers = 3,
    .heavy_work_us = 9000,
    .heavy_sleep_us = 1000,
    .light_work_us = 2000,
    .light_sleep_us = 8000,
    .restart_count = 5,
    .restart_window_s = 30,
    .nice_high = 0,
    .nice_low = 10
};

cfg_t cfg;

static void sigchld_handler(int signo){ got_sigchld = 1; }
static void term_handler(int signo){ got_term = 1; }
static void hup_handler(int signo){ got_hup = 1; }
static void usr1_handler(int signo){ got_usr1 = 1; }
static void usr2_handler(int signo){ got_usr2 = 1; }

int read_config(const char *path, cfg_t *out) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char *buf = NULL;
    size_t len = 0;
    ssize_t r = getdelim(&buf, &len, '\0', f);
    fclose(f);
    if (r <= 0) { free(buf); return -1; }
    // crude parsing: look for numbers after keywords
    cfg_t tmp = *out;
    char *p = buf;
    int v;
    if ((p = strstr(buf, "\"workers\"")))
        if (sscanf(p, "%*[^0-9]%d", &v) == 1) tmp.workers = v;
    if ((p = strstr(buf, "work_us")))
        if (sscanf(p, "%*[^0-9]%d", &v) == 1) tmp.heavy_work_us = v; // first match -> heavy
    // more careful parsing for heavy/light:
    char *ph = strstr(buf, "\"mode_heavy\"");
    if (ph && (p = strstr(ph, "work_us")))
        if (sscanf(p, "%*[^0-9]%d", &v) == 1) tmp.heavy_work_us = v;
    if (ph && (p = strstr(ph, "sleep_us")))
        if (sscanf(p, "%*[^0-9]%d", &v) == 1) tmp.heavy_sleep_us = v;
    char *pl = strstr(buf, "\"mode_light\"");
    if (pl && (p = strstr(pl, "work_us")))
        if (sscanf(p, "%*[^0-9]%d", &v) == 1) tmp.light_work_us = v;
    if (pl && (p = strstr(pl, "sleep_us")))
        if (sscanf(p, "%*[^0-9]%d", &v) == 1) tmp.light_sleep_us = v;
    // restart limits
    if ((p = strstr(buf, "\"restart_limit\""))){
        char *pc = strstr(p, "count");
        if (pc && sscanf(pc, "%*[^0-9]%d", &v) == 1) tmp.restart_count = v;
        char *pw = strstr(p, "window");
        if (pw && sscanf(pw, "%*[^0-9]%d", &v) == 1) tmp.restart_window_s = v;
    }
    // nice
    if ((p = strstr(buf, "\"nice\""))){
        char *ph = strstr(p, "\"high\"");
        if (ph && sscanf(ph, "%*[^0-9]%d", &v) == 1) tmp.nice_high = v;
        char *pl = strstr(p, "\"low\"");
        if (pl && sscanf(pl, "%*[^0-9]%d", &v) == 1) tmp.nice_low = v;
    }
    free(buf);
    *out = tmp;
    return 0;
}

int num_cpus_online() {
    long v = sysconf(_SC_NPROCESSORS_ONLN);
    return v > 0 ? (int)v : 1;
}

void spawn_worker_slot(int slot) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        // child: exec worker with args
        char id_arg[32], heavy_work[32], heavy_sleep[32], light_work[32], light_sleep[32];
        snprintf(id_arg, sizeof(id_arg), "%d", slot);
        snprintf(heavy_work, sizeof(heavy_work), "%d", cfg.heavy_work_us);
        snprintf(heavy_sleep, sizeof(heavy_sleep), "%d", cfg.heavy_sleep_us);
        snprintf(light_work, sizeof(light_work), "%d", cfg.light_work_us);
        snprintf(light_sleep, sizeof(light_sleep), "%d", cfg.light_sleep_us);
        char *argv[] = {"./worker", "--id", id_arg,
                        "--heavy-work", heavy_work, "--heavy-sleep", heavy_sleep,
                        "--light-work", light_work, "--light-sleep", light_sleep,
                        NULL};
        execv("./worker", argv);
        perror("execv ./worker");
        _exit(127);
    } else {
        // parent
        workers[slot].pid = pid;
        workers[slot].active = 1;
        workers[slot].slot = slot;
        // set nice: half workers get low priority (higher nice)
        int n = cfg.workers;
        int niceval = (slot < n/2) ? cfg.nice_low : cfg.nice_high;
        if (setpriority(PRIO_PROCESS, pid, niceval) != 0) {
            // warning only
            fprintf(stderr, "warn: setpriority(%d) -> %s\n", niceval, strerror(errno));
        }
        // affinity: round robin across CPUs
        int cpu = slot % num_cpus_online();
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu, &mask);
        if (sched_setaffinity(pid, sizeof(mask), &mask) != 0) {
            fprintf(stderr, "warn: sched_setaffinity(pid=%d, cpu=%d): %s\n", pid, cpu, strerror(errno));
        }
        printf("[supervisor] spawned worker slot=%d pid=%d nice=%d cpu=%d\n", slot, pid, niceval, cpu);
    }
}

void spawn_all_workers(int n) {
    for (int i = 0; i < n; ++i) {
        // if slot occupied and active, skip (used during reload we might want to create new set)
        if (workers[i].active) continue;
        spawn_worker_slot(i);
    }
}

void reap_children_and_restart() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // find which slot
        int slot = -1;
        for (int i = 0; i < cfg.workers; ++i) {
            if (workers[i].pid == pid) { slot = i; break; }
        }
        if (slot == -1) {
            // maybe old workers beyond cfg.workers
            for (int i = 0; i < MAX_WORKERS; ++i) if (workers[i].pid == pid) { slot = i; break; }
        }
        if (slot == -1) {
            printf("[supervisor] reaped unknown pid=%d\n", pid);
            continue;
        }
        workers[slot].active = 0;
        time_t now = time(NULL);
        workers[slot].restarts[workers[slot].restart_idx++ % RESTART_HISTORY] = now;
        int count = 0;
        for (int k = 0; k < RESTART_HISTORY; ++k) {
            time_t t = workers[slot].restarts[k];
            if (t > 0 && (now - t) <= cfg.restart_window_s) ++count;
        }
        printf("[supervisor] worker slot=%d pid=%d exited (status=%d). restarts in window=%d\n", slot, pid, status, count);
        if (count <= cfg.restart_count) {
            // restart
            printf("[supervisor] restarting slot=%d\n", slot);
            spawn_worker_slot(slot);
        } else {
            printf("[supervisor] too many restarts for slot=%d: suppressing restart until window passes\n", slot);
        }
    }
}

void broadcast_signal(int sig) {
    for (int i = 0; i < cfg.workers; ++i) {
        if (workers[i].active && workers[i].pid > 0) {
            kill(workers[i].pid, sig);
        }
    }
}

void orderly_shutdown() {
    printf("[supervisor] graceful shutdown requested\n");
    // send SIGTERM to all workers
    for (int i = 0; i < cfg.workers; ++i) {
        if (workers[i].active) {
            kill(workers[i].pid, SIGTERM);
        }
    }
    // wait up to 5 seconds
    time_t start = time(NULL);
    while (1) {
        int all_dead = 1;
        for (int i = 0; i < cfg.workers; ++i) {
            if (workers[i].active) {
                // check if still alive
                if (kill(workers[i].pid, 0) == 0) { all_dead = 0; break; }
                else workers[i].active = 0;
            }
        }
        if (all_dead) break;
        if (time(NULL) - start >= 5) {
            printf("[supervisor] timeout waiting for workers, sending SIGKILL\n");
            for (int i = 0; i < cfg.workers; ++i)
                if (workers[i].active) kill(workers[i].pid, SIGKILL);
            break;
        }
        sleep(1);
    }
    printf("[supervisor] shutdown complete\n");
}

void do_reload() {
    printf("[supervisor] reload requested (SIGHUP)\n");
    cfg_t newcfg = cfg;
    if (read_config("config.json", &newcfg) == 0) {
        printf("[supervisor] loaded new config: workers=%d heavy=%d/%d light=%d/%d\n",
               newcfg.workers, newcfg.heavy_work_us, newcfg.heavy_sleep_us, newcfg.light_work_us, newcfg.light_sleep_us);
    } else {
        printf("[supervisor] config.json not found or unreadable, keeping existing config\n");
    }
    // spawn new set in free slots (if increased workers) OR spawn into new indices
    int old_n = cfg.workers;
    cfg = newcfg;
    // if increased, spawn into new slots
    for (int i = 0; i < cfg.workers; ++i) {
        if (!workers[i].active) spawn_worker_slot(i);
    }
    // after new workers started, gracefully ask old extras to stop (if reduced)
    if (cfg.workers < old_n) {
        for (int i = cfg.workers; i < old_n; ++i) {
            if (workers[i].active) {
                kill(workers[i].pid, SIGTERM);
            }
        }
    }
    // alternatively: a rolling restart could be implemented here.
}

int main(int argc, char **argv) {
    cfg = cfg_default;
    read_config("config.json", &cfg); // ignore error -> use defaults

    // init workers array
    memset(workers, 0, sizeof(workers));
    for (int i = 0; i < cfg.workers; ++i) workers[i].slot = i;

    // install handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);
    sa.sa_handler = term_handler;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sa.sa_handler = hup_handler;
    sigaction(SIGHUP, &sa, NULL);
    sa.sa_handler = usr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = usr2_handler;
    sigaction(SIGUSR2, &sa, NULL);

    // spawn initial workers
    for (int i = 0; i < cfg.workers; ++i) spawn_worker_slot(i);

    // main loop: handle flags and reap children
    while (!got_term) {
        if (got_sigchld) {
            got_sigchld = 0;
            reap_children_and_restart();
        }
        if (got_usr1) {
            got_usr1 = 0;
            printf("[supervisor] broadcasting SIGUSR1 (light)\n");
            broadcast_signal(SIGUSR1);
        }
        if (got_usr2) {
            got_usr2 = 0;
            printf("[supervisor] broadcasting SIGUSR2 (heavy)\n");
            broadcast_signal(SIGUSR2);
        }
        if (got_hup) {
            got_hup = 0;
            do_reload();
        }
        // small sleep to not busy loop
        sleep(1);
    }

    // got_term set -> graceful shutdown
    orderly_shutdown();
    return 0;
}

