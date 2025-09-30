#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t current_mode = 0;

typedef struct {
    int work_us;
    int sleep_us;
} mode_config;

mode_config heavy_mode = {9000, 1000};
mode_config light_mode = {2000, 8000};

void signal_handler(int sig) {
    switch(sig) {
        case SIGTERM:
            keep_running = 0;
            break;
        case SIGUSR1:
            current_mode = 1;
            break;
        case SIGUSR2:
            current_mode = 0;
            break;
    }
}

void setup_signals() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
}

void cpu_work_simple(int microseconds) {
    struct timespec start, current;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime failed");
        return;
    }

    long nanoseconds = microseconds * 1000L;
    volatile unsigned long counter __attribute__((unused)) = 0;

    while (1) {
        if (clock_gettime(CLOCK_MONOTONIC, &current) == -1) {
            perror("clock_gettime failed");
            break;
        }

        long elapsed = (current.tv_sec - start.tv_sec) * 1000000000L +
            (current.tv_nsec - start.tv_nsec);

        if (elapsed >= nanoseconds) {
            break;
        }

        for (int i = 0; i < 100; i++) {
            counter += i * 3;
        }
    }
}

void cpu_work(int microseconds) {
    cpu_work_simple(microseconds);
}

int main(void) {
    printf("Worker started with PID: %d\n", getpid());
    setup_signals();

    int tick_count = 0;

    while(keep_running) {
        mode_config *current_config = current_mode ? &light_mode : &heavy_mode;

        printf("Worker %d: tick=%d, mode=%s, work_us=%d, sleep_us=%d\n",
               getpid(), tick_count,
               current_mode ? "LIGHT" : "HEAVY",
               current_config->work_us,
               current_config->sleep_us);
        fflush(stdout);

        cpu_work(current_config->work_us);

        usleep(current_config->sleep_us);

        tick_count++;
    }

    printf("Worker %d: Shutting down gracefully\n", getpid());
    return 0;
}