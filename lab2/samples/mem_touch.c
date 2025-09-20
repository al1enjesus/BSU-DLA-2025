#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;
static volatile sig_atomic_t add_step = 0;
static volatile sig_atomic_t remove_step = 0;

static void handle_sigterm(int sig) { (void)sig; stop_requested = 1; }
static void handle_sigusr1(int sig) { (void)sig; add_step = 1; }
static void handle_sigusr2(int sig) { (void)sig; remove_step = 1; }

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [--rss-mb N] [--step-mb N] [--sleep-ms N] [--set-rlimit-as MB]\\n"
            "Signals: SIGUSR1 -> allocate +step, SIGUSR2 -> free -step, SIGTERM -> stop\\n",
            prog);
}

static void *allocate_mb(size_t mb) {
    size_t bytes = mb * 1024UL * 1024UL;
    void *p = malloc(bytes);
    if (!p) return NULL;
    memset(p, 0xA5, bytes); // touch pages
    return p;
}

static void free_block(void **p) {
    if (*p) { free(*p); *p = NULL; }
}

static void maybe_set_rlimit_as(long mb) {
    if (mb <= 0) return;
    struct rlimit rl;
    rl.rlim_cur = (rlim_t)mb * 1024UL * 1024UL;
    rl.rlim_max = rl.rlim_cur;
    if (setrlimit(RLIMIT_AS, &rl) == -1) {
        perror("setrlimit(RLIMIT_AS)");
    }
}

int main(int argc, char **argv) {
    long target_mb = 512;
    long step_mb = 64;
    long sleep_ms = 200;
    long rlimit_as_mb = 0; // 0=disabled

    static struct option opts[] = {
        {"rss-mb", required_argument, 0, 'r'},
        {"step-mb", required_argument, 0, 's'},
        {"sleep-ms", required_argument, 0, 't'},
        {"set-rlimit-as", required_argument, 0, 'l'},
        {0,0,0,0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (c) {
            case 'r': target_mb = atol(optarg); break;
            case 's': step_mb = atol(optarg); break;
            case 't': sleep_ms = atol(optarg); break;
            case 'l': rlimit_as_mb = atol(optarg); break;
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

    maybe_set_rlimit_as(rlimit_as_mb);

    size_t capacity = (size_t)((target_mb + step_mb) / step_mb) + 8;
    void **blocks = calloc(capacity, sizeof(void*));
    size_t count = 0;
    long allocated_mb = 0;

    fprintf(stdout, "mem_touch start: pid=%d target=%ldMB step=%ldMB sleep=%ldms\n",
            getpid(), target_mb, step_mb, sleep_ms);
    fflush(stdout);

    while (!stop_requested) {
        if (allocated_mb < target_mb && count < capacity) {
            void *p = allocate_mb((size_t)step_mb);
            if (!p) {
                perror("malloc");
                break;
            }
            blocks[count++] = p;
            allocated_mb += step_mb;
        }

        if (add_step && count < capacity) {
            add_step = 0;
            void *p = allocate_mb((size_t)step_mb);
            if (p) { blocks[count++] = p; allocated_mb += step_mb; }
        }
        if (remove_step && count > 0) {
            remove_step = 0;
            free_block(&blocks[--count]);
            allocated_mb -= step_mb;
        }

        fprintf(stdout, "rss_target=%ldMB allocated=%ldMB blocks=%zu\n",
                target_mb, allocated_mb, count);
        fflush(stdout);

        struct timespec ts = { .tv_sec = sleep_ms / 1000, .tv_nsec = (sleep_ms % 1000) * 1000000 };
        while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
    }

    for (size_t i = 0; i < count; i++) free_block(&blocks[i]);
    free(blocks);
    fprintf(stdout, "mem_touch stop: pid=%d\n", getpid());
    fflush(stdout);
    return 0;
}


