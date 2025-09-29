#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sched.h>
#include <stdint.h>

static volatile sig_atomic_t mode_heavy = 1;     // 1=heavy, 0=light
static volatile sig_atomic_t stop_requested = 0; // graceful stop

static long heavy_work_us, heavy_sleep_us;
static long light_work_us, light_sleep_us;
static int nice_value;
static int cpu_affinity;

static void signal_handler(int sig) {
    switch (sig) {
        case SIGTERM:
        case SIGINT:
            stop_requested = 1;
            break;
        case SIGUSR1:
            mode_heavy = 0; // light
            break;
        case SIGUSR2:
            mode_heavy = 1; // heavy
            break;
    }
}

static void setup_signals(void) {
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
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
        // retry on signal interruption
    }
}

static void set_nice_priority(int nice_val) {
    if (setpriority(PRIO_PROCESS, 0, nice_val) == -1) {
        perror("setpriority");
    }
}

static void set_cpu_affinity(int cpu) {
    if (cpu < 0) return;
    
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((unsigned)cpu, &set);
    
    if (sched_setaffinity(0, sizeof(set), &set) == -1) {
        perror("sched_setaffinity");
    }
}

static int get_current_cpu(void) {
    return sched_getcpu();
}

static void print_worker_info(void) {
    int current_cpu = get_current_cpu();
    const char *mode_str = mode_heavy ? "heavy" : "light";
    long work_us = mode_heavy ? heavy_work_us : light_work_us;
    long sleep_us = mode_heavy ? heavy_sleep_us : light_sleep_us;
    
    printf("[worker %d] mode=%s, work=%ld us, sleep=%ld us, nice=%d, cpu=%d, current_cpu=%d\n",
           getpid(), mode_str, work_us, sleep_us, nice_value, cpu_affinity, current_cpu);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <heavy_work_us> <heavy_sleep_us> <light_work_us> <light_sleep_us> <nice> <cpu>\n", argv[0]);
        return 1;
    }
    
    heavy_work_us = atol(argv[1]);
    heavy_sleep_us = atol(argv[2]);
    light_work_us = atol(argv[3]);
    light_sleep_us = atol(argv[4]);
    nice_value = atoi(argv[5]);
    cpu_affinity = atoi(argv[6]);
    
    setup_signals();
    
    // Устанавливаем nice и CPU affinity
    set_nice_priority(nice_value);
    set_cpu_affinity(cpu_affinity);
    
    printf("[worker %d] Started: heavy=[%ld/%ld us], light=[%ld/%ld us], nice=%d, cpu=%d\n",
           getpid(), heavy_work_us, heavy_sleep_us, light_work_us, light_sleep_us, 
           nice_value, cpu_affinity);
    fflush(stdout);
    
    time_t last_info_time = 0;
    unsigned long tick_count = 0;
    
    while (!stop_requested) {
        int heavy = mode_heavy;
        long work_us = heavy ? heavy_work_us : light_work_us;
        long sleep_us = heavy ? heavy_sleep_us : light_sleep_us;
        
        // Выполняем "работу"
        busy_work((uint64_t)work_us * 400); // калибровка условная
        
        // Спим
        nanosleep_us(sleep_us);
        
        tick_count++;
        
        // Выводим информацию каждые 3 секунды
        time_t now = time(NULL);
        if (now - last_info_time >= 3) {
            print_worker_info();
            printf("[worker %d] tick_count=%lu\n", getpid(), tick_count);
            fflush(stdout);
            last_info_time = now;
        }
    }
    
    printf("[worker %d] Stopping gracefully after %lu ticks\n", getpid(), tick_count);
    fflush(stdout);
    
    return 0;
}