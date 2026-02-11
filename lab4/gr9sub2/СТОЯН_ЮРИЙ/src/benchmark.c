/**
 * benchmark.c - Улучшенная версия бенчмарка
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <x86intrin.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#define ITERATIONS 1000000

 // Автоматическое определение частоты CPU
double get_cpu_frequency() {
    static double freq = 0;
    if (freq == 0) {
        FILE* fp = fopen("/proc/cpuinfo", "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "cpu MHz")) {
                    sscanf(line, "cpu MHz : %lf", &freq);
                    freq /= 1000.0; // Convert MHz to GHz
                    break;
                }
            }
            fclose(fp);
        }
        if (freq == 0) freq = 2.5; // fallback
    }
    return freq;
}

uint64_t measure_cycles(void (*func)()) {
    uint64_t start, end;

    // Прогрев кэша
    for (int i = 0; i < 1000; i++) {
        func();
    }

    start = __rdtsc();
    for (int i = 0; i < ITERATIONS; i++) {
        func();
    }
    end = __rdtsc();

    return (end - start) / ITERATIONS;
}

__attribute__((noinline))
int dummy() {
    static volatile int counter = 0;
    return counter++;
}

void getpid_call() {
    volatile pid_t pid = getpid();
    (void)pid;
}

void open_close_call() {
    static int file_counter = 0;
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "/tmp/benchmark_test_%d", file_counter++);

    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd != -1) {
        close(fd);
        unlink(path);
    }
}

void open_close_cached_call() {
    static const char* path = "/tmp/benchmark_test_cached";

    // Создаем файл один раз
    static int initialized = 0;
    if (!initialized) {
        int fd = open(path, O_WRONLY | O_CREAT, 0644);
        if (fd != -1) close(fd);
        initialized = 1;
    }

    int fd = open(path, O_RDONLY);
    if (fd != -1) {
        close(fd);
    }
}

void gettimeofday_call() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
}

int main() {
    double cpu_freq = get_cpu_frequency();

    printf("=== SYSTEM CALL BENCHMARK ===\n");
    printf("CPU Frequency: %.2f GHz\n", cpu_freq);
    printf("Iterations: %d\n\n", ITERATIONS);

    uint64_t dummy_cycles = measure_cycles((void (*)())dummy);
    uint64_t getpid_cycles = measure_cycles(getpid_call);
    uint64_t open_close_cycles = measure_cycles(open_close_call);
    uint64_t open_close_cached_cycles = measure_cycles(open_close_cached_call);
    uint64_t gettimeofday_cycles = measure_cycles(gettimeofday_call);

    double dummy_ns = (double)dummy_cycles / cpu_freq;
    double getpid_ns = (double)getpid_cycles / cpu_freq;
    double open_close_ns = (double)open_close_cycles / cpu_freq;
    double open_close_cached_ns = (double)open_close_cached_cycles / cpu_freq;
    double gettimeofday_ns = (double)gettimeofday_cycles / cpu_freq;

    printf("RESULTS:\n");
    printf("--------------------------------------------------------------------------------------\n");
    printf("| Operation                 | Avg. Cycles | Avg. Time (ns) | Slower than userspace   |\n");
    printf("--------------------------------------------------------------------------------------\n");
    printf("| dummy() userspace         | %-11lu | %-14.2f | 1.00x                   |\n",
        dummy_cycles, dummy_ns);
    printf("| gettimeofday() vDSO       | %-11lu | %-14.2f | %.2fx                  |\n",
        gettimeofday_cycles, gettimeofday_ns, (double)gettimeofday_cycles / dummy_cycles);
    printf("| getpid()                  | %-11lu | %-14.2f | %.2fx                  |\n",
        getpid_cycles, getpid_ns, (double)getpid_cycles / dummy_cycles);
    printf("| open+close() (cached)     | %-11lu | %-14.2f | %.2fx                |\n",
        open_close_cached_cycles, open_close_cached_ns, (double)open_close_cached_cycles / dummy_cycles);
    printf("| open+close() (new file)   | %-11lu | %-14.2f | %.2fx                |\n",
        open_close_cycles, open_close_ns, (double)open_close_cycles / dummy_cycles);
    printf("--------------------------------------------------------------------------------------\n");

    // Очистка
    unlink("/tmp/benchmark_test_cached");

    return 0;
}