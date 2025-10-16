/**
 * benchmark_cold_cache.c - Benchmark с холодным кэшем
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

#define ITERATIONS 100000
#define CACHE_COOL_SIZE (100 * 1024 * 1024)

double get_cpu_frequency() {
    static double freq = 0;
    if (freq == 0) {
        FILE* fp = fopen("/proc/cpuinfo", "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strstr(line, "cpu MHz")) {
                    sscanf(line, "cpu MHz : %lf", &freq);
                    freq /= 1000.0;
                    break;
                }
            }
            fclose(fp);
        }
        if (freq == 0) freq = 2.5;
    }
    return freq;
}

// Функция для охлаждения CPU кэша
void cool_cpu_cache() {
    char* cool_data = malloc(CACHE_COOL_SIZE);
    for (int i = 0; i < CACHE_COOL_SIZE; i += 64) {
        cool_data[i] = i % 256;
    }
    free(cool_data);
    _mm_mfence(); // Барьер памяти
}

// Функция для охлаждения page cache (без sudo)
void cool_page_cache() {
    // Создаем и сразу удаляем большие файлы чтобы вытеснить кэш
    for (int i = 0; i < 10; i++) {
        char cmd[100];
        snprintf(cmd, sizeof(cmd),
            "dd if=/dev/zero of=/tmp/cool_cache_%d bs=1M count=50 status=none 2>/dev/null", i);
        system(cmd);
    }
    system("rm -f /tmp/cool_cache_* 2>/dev/null");
}

uint64_t measure_cycles_cold(void (*func)(), int iterations) {
    uint64_t total = 0;

    for (int i = 0; i < iterations; i++) {
        cool_cpu_cache(); // Охлаждаем CPU кэш перед каждым измерением

        uint64_t start = __rdtsc();
        func();
        uint64_t end = __rdtsc();
        total += (end - start);
    }

    return total / iterations;
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

void open_close_cached_call() {
    static const char* path = "/tmp/benchmark_test_cached";
    static int initialized = 0;

    if (!initialized) {
        int fd = open(path, O_WRONLY | O_CREAT, 0644);
        if (fd != -1) close(fd);
        initialized = 1;
    }

    cool_page_cache(); // Охлаждаем page cache перед файловой операцией

    int fd = open(path, O_RDONLY);
    if (fd != -1) close(fd);
}

void open_close_new_call() {
    static int file_counter = 0;
    char path[PATH_MAX];

    cool_page_cache(); // Охлаждаем page cache

    snprintf(path, sizeof(path), "/tmp/bench_temp_%d", file_counter++ % 100);

    int fd = open(path, O_WRONLY | O_CREAT, 0644);
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

    printf("=== SYSTEM CALL BENCHMARK - COLD CACHE ===\n");
    printf("CPU Frequency: %.2f GHz\n", cpu_freq);
    printf("Iterations: %d\n", ITERATIONS);
    printf("Cache state: COLD (forced cache misses)\n\n");

    // Измеряем с холодным кэшем
    uint64_t dummy_cycles = measure_cycles_cold((void (*)())dummy, ITERATIONS);
    uint64_t getpid_cycles = measure_cycles_cold(getpid_call, ITERATIONS);
    uint64_t gettimeofday_cycles = measure_cycles_cold(gettimeofday_call, ITERATIONS);
    uint64_t open_close_cached_cycles = measure_cycles_cold(open_close_cached_call, 10000); // Меньше итераций для файлов
    uint64_t open_close_new_cycles = measure_cycles_cold(open_close_new_call, 1000); // Еще меньше для новых файлов

    double dummy_ns = (double)dummy_cycles / cpu_freq;
    double getpid_ns = (double)getpid_cycles / cpu_freq;
    double gettimeofday_ns = (double)gettimeofday_cycles / cpu_freq;
    double open_close_cached_ns = (double)open_close_cached_cycles / cpu_freq;
    double open_close_new_ns = (double)open_close_new_cycles / cpu_freq;

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
        open_close_new_cycles, open_close_new_ns, (double)open_close_new_cycles / dummy_cycles);
    printf("--------------------------------------------------------------------------------------\n");

    // Очистка
    system("rm -f /tmp/benchmark_test_cached /tmp/bench_temp_* 2>/dev/null");

    return 0;
}