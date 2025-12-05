#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sched.h>
#include <errno.h>

volatile sig_atomic_t do_terminate = 0;
volatile sig_atomic_t mode_light = 0;

static void term_handler(int s){ do_terminate = 1; }
static void usr1_handler(int s){ mode_light = 1; }
static void usr2_handler(int s){ mode_light = 0; }

static long timespec_to_us(struct timespec a, struct timespec b) {
    return (a.tv_sec - b.tv_sec) * 1000000L + (a.tv_nsec - b.tv_nsec) / 1000L;
}

void busy_work_us(int work_us) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (timespec_to_us(now, start) >= work_us) break;
        // busy loop
    }
}

int main(int argc, char **argv) {
    int id = -1;
    int heavy_work = 9000, heavy_sleep = 1000, light_work = 2000, light_sleep = 8000;
    // crude arg parse
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--id") == 0 && i+1 < argc) { id = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--heavy-work") == 0 && i+1 < argc) heavy_work = atoi(argv[++i]);
        else if (strcmp(argv[i], "--heavy-sleep") == 0 && i+1 < argc) heavy_sleep = atoi(argv[++i]);
        else if (strcmp(argv[i], "--light-work") == 0 && i+1 < argc) light_work = atoi(argv[++i]);
        else if (strcmp(argv[i], "--light-sleep") == 0 && i+1 < argc) light_sleep = atoi(argv[++i]);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = term_handler; sigaction(SIGTERM, &sa, NULL);
    sa.sa_handler = usr1_handler; sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = usr2_handler; sigaction(SIGUSR2, &sa, NULL);

    pid_t pid = getpid();
    printf("[worker] pid=%d id=%d starting heavy=%d/%d light=%d/%d\n",
           pid, id, heavy_work, heavy_sleep, light_work, light_sleep);
    fflush(stdout);

    unsigned long iterations = 0;
    unsigned long total_work_us = 0;
    time_t last_report = time(NULL);

    while (!do_terminate) {
        int work_us = mode_light ? light_work : heavy_work;
        int sleep_us = mode_light ? light_sleep : heavy_sleep;
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        busy_work_us(work_us);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        long did = (t1.tv_sec - t0.tv_sec) * 1000000L + (t1.tv_nsec - t0.tv_nsec)/1000L;
        total_work_us += (unsigned long) did;
        iterations++;
        // sleep for the configured interval
        usleep(sleep_us);
        // periodic report
        time_t now = time(NULL);
        if (now - last_report >= 2) {
            int cpu = sched_getcpu();
            printf("[worker] pid=%d id=%d mode=%s iter=%lu total_work_us=%lu cpu=%d\n",
                   pid, id, mode_light ? "light" : "heavy", iterations, total_work_us, cpu);
            fflush(stdout);
            last_report = now;
        }
    }

    printf("[worker] pid=%d id=%d terminating gracefully (iters=%lu total_work_us=%lu)\n",
           pid, id, iterations, total_work_us);
    fflush(stdout);
    return 0;
}

