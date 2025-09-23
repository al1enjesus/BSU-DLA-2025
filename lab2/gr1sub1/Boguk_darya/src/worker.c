#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sched.h>
#include <sys/types.h>

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t mode_heavy = 1;

static int work_us_heavy = 9000;
static int sleep_us_heavy = 1000;
static int work_us_light = 2000;
static int sleep_us_light = 8000;

void handle_sigterm(int signo) { running = 0; }
void handle_sigusr1(int signo) { mode_heavy = 0; }
void handle_sigusr2(int signo) { mode_heavy = 1; }

void busy_work(int usec) {
    volatile unsigned long x = 0;
    struct timespec ts_start, ts_now;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);
    while (1) {
        x++;
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        long diff = (ts_now.tv_sec - ts_start.tv_sec) * 1000000L +
                    (ts_now.tv_nsec - ts_start.tv_nsec) / 1000;
        if (diff >= usec) break;
    }
}

int main(int argc, char *argv[]) {
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGUSR2, handle_sigusr2);

    int tick = 0;
    while (running) {
        tick++;
        if (mode_heavy) {
            busy_work(work_us_heavy);
            usleep(sleep_us_heavy);
        } else {
            busy_work(work_us_light);
            usleep(sleep_us_light);
        }
        printf("[Worker %d] tick=%d mode=%s CPU=%d\n",
               (int)getpid(), tick,
               mode_heavy ? "heavy" : "light",
               sched_getcpu());
        fflush(stdout);
    }
    printf("[Worker %d] exiting gracefully\n", getpid());
    return 0;
}
