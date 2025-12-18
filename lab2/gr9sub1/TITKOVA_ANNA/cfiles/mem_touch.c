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
            "Usage: %s [--rss-mb N] [--step-mb N] [--sleep-ms N] [--limit-as-mb MB]\n"
            "Signals: SIGUSR1 -> allocate +step, SIGUSR2 -> free -step, SIGTERM -> stop\n",
            prog);
}

// Функция для вывода статуса памяти из /proc
static void print_memory_status(const char *tag) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) { 
        perror("fopen /proc/self/status"); 
        return; 
    }
    char line[256];
    printf("=== %s: /proc/self/status ===\n", tag);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmSize:", 7) == 0 || strncmp(line, "VmRSS:", 6) == 0 || 
            strncmp(line, "VmPeak:", 7) == 0 || strncmp(line, "VmHWM:", 6) == 0) {
            fputs(line, stdout);
        }
    }
    fclose(f);
    fflush(stdout);
}

static void *allocate_mb(size_t mb) {
    size_t bytes = mb * 1024UL * 1024UL;
    void *p = malloc(bytes);
    if (!p) return NULL;
    // "Трогаем" память чтобы гарантировать выделение физической памяти
    memset(p, 0xA5, bytes);
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
        fprintf(stderr, "Failed to set RLIMIT_AS = %ld MB\n", mb);
    } else {
        printf("Successfully set RLIMIT_AS = %ld MB\n", mb);
        fflush(stdout);
    }
}

int main(int argc, char **argv) {
    long target_mb = 256;  // Уменьшим целевой размер по умолчанию
    long step_mb = 32;     // Уменьшим шаг
    long sleep_ms = 500;
    long limit_as_mb = 0; // 0=disabled

    static struct option opts[] = {
        {"rss-mb", required_argument, 0, 'r'},
        {"step-mb", required_argument, 0, 's'},
        {"sleep-ms", required_argument, 0, 't'},
        {"limit-as-mb", required_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {0,0,0,0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "", opts, NULL)) != -1) {
        switch (c) {
            case 'r': target_mb = atol(optarg); break;
            case 's': step_mb = atol(optarg); break;
            case 't': sleep_ms = atol(optarg); break;
            case 'l': limit_as_mb = atol(optarg); break;
            case 'h': print_usage(argv[0]); return 0;
            default: print_usage(argv[0]); return 1;
        }
    }

    // Устанавливаем обработчики сигналов
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

    // Устанавливаем лимит памяти если указан
    maybe_set_rlimit_as(limit_as_mb);

    // Выделяем массив для блоков памяти
    size_t capacity = (size_t)((target_mb + step_mb) / step_mb) + 16;
    void **blocks = calloc(capacity, sizeof(void*));
    if (!blocks) {
        perror("calloc blocks array");
        return 2;
    }
    
    size_t count = 0;
    long allocated_mb = 0;

    printf("mem_touch started: pid=%d target=%ldMB step=%ldMB sleep=%ldms limit=%ldMB\n",
            getpid(), target_mb, step_mb, sleep_ms, limit_as_mb);
    print_memory_status("start");
    fflush(stdout);

    int allocation_failed = 0;

    while (!stop_requested) {
        // Основное выделение памяти до целевого размера
        if (!allocation_failed && allocated_mb < target_mb && count < capacity) {
            void *p = allocate_mb((size_t)step_mb);
            if (!p) {
                perror("malloc failed");
                print_memory_status("malloc-failed");
                allocation_failed = 1;
                fprintf(stderr, "Allocation failed at %ld MB (limit: %ld MB)\n", 
                        allocated_mb, limit_as_mb);
            } else {
                blocks[count++] = p;
                allocated_mb += step_mb;
                printf("[mem_touch] allocated +%ldMB -> total: %ldMB\n", 
                       step_mb, allocated_mb);
                if (count % 2 == 0) { // Периодически выводим статус
                    print_memory_status("progress");
                }
            }
        }

        // Обработка сигналов для ручного управления
        if (add_step && count < capacity && !allocation_failed) {
            add_step = 0;
            void *p = allocate_mb((size_t)step_mb);
            if (p) { 
                blocks[count++] = p; 
                allocated_mb += step_mb;
                printf("[mem_touch] SIGUSR1: allocated +%ldMB -> total: %ldMB\n", 
                       step_mb, allocated_mb);
            } else {
                perror("SIGUSR1 malloc failed");
                allocation_failed = 1;
            }
        }
        
        if (remove_step && count > 0) {
            remove_step = 0;
            free_block(&blocks[--count]);
            allocated_mb -= step_mb;
            allocation_failed = 0; // Сброс флага при освобождении памяти
            printf("[mem_touch] SIGUSR2: freed -%ldMB -> total: %ldMB\n", 
                   step_mb, allocated_mb);
        }

        // Спим
        if (sleep_ms > 0) {
            struct timespec ts = { 
                .tv_sec = sleep_ms / 1000, 
                .tv_nsec = (sleep_ms % 1000) * 1000000 
            };
            while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
        }
    }

    // Освобождаем всю память
    printf("Cleaning up: freeing %zu blocks (%ld MB)\n", count, allocated_mb);
    for (size_t i = 0; i < count; i++) {
        free_block(&blocks[i]);
    }
    free(blocks);
    
    print_memory_status("end");
    printf("mem_touch stopped: pid=%d\n", getpid());
    fflush(stdout);
    
    return allocation_failed ? 3 : 0;
}