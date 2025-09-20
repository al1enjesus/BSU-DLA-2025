#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t mode_heavy = 1;     // 1=heavy, 0=light
static volatile sig_atomic_t stop_requested = 0; // graceful stop

static void handle_sigterm(int sig) {
    (void)sig;
    stop_requested = 1;
}

static void handle_sigusr1(int sig) {
    (void)sig;
    mode_heavy = 0; // light
}

static void handle_sigusr2(int sig) {
    (void)sig;
    mode_heavy = 1; // heavy
}

static void busy_work(uint64_t iterations) {
    volatile uint64_t x = 0;
    for (uint64_t i = 0; i < iterations; i++) {
        x += i ^ (x << 1);
    }
    (void)x;
}

static void nanosleep_us(long usec) {
    struct timespec ts;
    ts.tv_sec = usec / 1000000;
    ts.tv_nsec = (usec % 1000000) * 1000;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        // retry
    }
}

// CPU affinity: only on Linux; on other OS do nothing (compiles everywhere)
#ifdef __linux__
#include <sched.h>
static void set_affinity_optional(int cpu) {
    if (cpu < 0) return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((unsigned)cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity");
    }
}
#else
static void set_affinity_optional(int cpu) {
    (void)cpu; // not supported on this platform
}
#endif

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [--work-us N] [--sleep-us N] [--light-work-us N] [--light-sleep-us N]\\n"
            "          [--duration SEC] [--cpu CPU]\\n"
            "Signals: SIGUSR1 -> light, SIGUSR2 -> heavy, SIGTERM/SIGINT -> stop\\n",
            prog);
}

int main(int argc, char **argv) {
    long work_us_heavy = 9000;
    long sleep_us_heavy = 1000;
    long work_us_light = 2000;
    long sleep_us_light = 8000;
    int duration_sec = 0; // 0 = infinite
    int pin_cpu = -1;     // -1 = no pin

    static struct option opts[] = {
        {"work-us", required_argument, 0, 'w'},
        {"sleep-us", required_argument, 0, 's'},
        {"light-work-us", required_argument, 0, 'W'},
        {"light-sleep-us", required_argument, 0, 'S'},
        {"duration", required_argument, 0, 'd'},
        {"cpu", required_argument, 0, 'c'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (c) {
            case 'w': work_us_heavy = atol(optarg); break;
            case 's': sleep_us_heavy = atol(optarg); break;
            case 'W': work_us_light = atol(optarg); break;
            case 'S': sleep_us_light = atol(optarg); break;
            case 'd': duration_sec = atoi(optarg); break;
            case 'c': pin_cpu = atoi(optarg); break;
            default: print_usage(argv[0]); return 1;
        }
    }

    struct sigaction sa = {0};
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    struct sigaction su1 = {0};
    su1.sa_handler = handle_sigusr1;
    sigemptyset(&su1.sa_mask);
    sigaction(SIGUSR1, &su1, NULL);

    struct sigaction su2 = {0};
    su2.sa_handler = handle_sigusr2;
    sigemptyset(&su2.sa_mask);
    sigaction(SIGUSR2, &su2, NULL);

    set_affinity_optional(pin_cpu);

    fprintf(stdout,
            "cpu_burn start: pid=%d, cpu=%d, heavy=[%ld/%ld us], light=[%ld/%ld us]\n",
            getpid(), pin_cpu, work_us_heavy, sleep_us_heavy, work_us_light, sleep_us_light);
    fflush(stdout);

    time_t t0 = time(NULL);
    while (!stop_requested) {
        int heavy = mode_heavy;
        long w = heavy ? work_us_heavy : work_us_light;
        long s = heavy ? sleep_us_heavy : sleep_us_light;

        busy_work((uint64_t)w * 400); // калибровка условная
        nanosleep_us(s);

        if (duration_sec > 0 && (time(NULL) - t0) >= duration_sec) break;
        if ((time(NULL) % 2) == 0) {
            fprintf(stdout, "tick pid=%d mode=%s\n", getpid(), heavy ? "heavy" : "light");
            fflush(stdout);
        }
    }

    fprintf(stdout, "cpu_burn stop: pid=%d\n", getpid());
    fflush(stdout);
    return 0;
}


