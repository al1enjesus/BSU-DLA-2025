#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <x86intrin.h>

#define FAST_ITERATIONS 1000000UL
#define SLOW_ITERATIONS 1000UL

__attribute__((noinline)) int dummy_function() {
    return 42;
}

uint64_t safe_measure_rdtsc(void (*func)(), uint64_t iterations) {
    uint64_t total_cycles = 0;

    for (int run = 0; run < 3; run++) {
        uint64_t start = __rdtsc();
        func();
        uint64_t end = __rdtsc();

        if ((end - start) / iterations > total_cycles || run == 0) {
            total_cycles = (end - start) / iterations;
        }
    }

    return total_cycles;
}

uint64_t safe_measure_time(void (*func)(), uint64_t iterations, int clock_type) {
    struct timespec start, end;
    uint64_t total_ns = 0;

    for (int run = 0; run < 3; run++) {
        clock_gettime(clock_type, &start);
        func();
        clock_gettime(clock_type, &end);

        uint64_t ns_start = (uint64_t)start.tv_sec * 1000000000ULL + (uint64_t)start.tv_nsec;
        uint64_t ns_end = (uint64_t)end.tv_sec * 1000000000ULL + (uint64_t)end.tv_nsec;
        uint64_t avg_ns = (ns_end - ns_start) / iterations;

        if (avg_ns > total_ns || run == 0) {
            total_ns = avg_ns;
        }
    }

    return total_ns;
}

void test_dummy_fast() {
    volatile int result;
    for (uint64_t i = 0; i < FAST_ITERATIONS; i++) {
        result = dummy_function();
    }
    (void)result;
}

void test_getpid_fast() {
    volatile pid_t result;
    for (uint64_t i = 0; i < FAST_ITERATIONS; i++) {
        result = getpid();
    }
    (void)result;
}

void test_gettimeofday_fast() {
    struct timeval tv;
    for (uint64_t i = 0; i < FAST_ITERATIONS; i++) {
        gettimeofday(&tv, NULL);
    }
}

void test_clock_gettime_fast() {
    struct timespec ts;
    for (uint64_t i = 0; i < FAST_ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
    }
}

void test_open_close_slow() {
    const char* test_file = "/tmp/benchmark_test_file";
    for (uint64_t i = 0; i < SLOW_ITERATIONS; i++) {
        int fd = open(test_file, O_RDWR | O_CREAT, 0644);
        if (fd != -1) {
            close(fd);
        }
    }
    unlink(test_file);
}

int main() {
    printf("Benchmark системных вызовов\n");
    printf("============================\n\n");
    printf("Конфигурация:\n");
    printf("- Быстрые операции: %lu итераций\n", FAST_ITERATIONS);
    printf("- Медленные операции: %lu итераций\n\n", SLOW_ITERATIONS);

    printf("Измерение...\n");

    uint64_t cycles_dummy = safe_measure_rdtsc(test_dummy_fast, FAST_ITERATIONS);
    uint64_t time_dummy_mono = safe_measure_time(test_dummy_fast, FAST_ITERATIONS, CLOCK_MONOTONIC);
    uint64_t time_dummy_cpu = safe_measure_time(test_dummy_fast, FAST_ITERATIONS, CLOCK_PROCESS_CPUTIME_ID);

    uint64_t cycles_getpid = safe_measure_rdtsc(test_getpid_fast, FAST_ITERATIONS);
    uint64_t time_getpid_mono = safe_measure_time(test_getpid_fast, FAST_ITERATIONS, CLOCK_MONOTONIC);
    uint64_t time_getpid_cpu = safe_measure_time(test_getpid_fast, FAST_ITERATIONS, CLOCK_PROCESS_CPUTIME_ID);

    uint64_t cycles_gettime = safe_measure_rdtsc(test_gettimeofday_fast, FAST_ITERATIONS);
    uint64_t time_gettime_mono = safe_measure_time(test_gettimeofday_fast, FAST_ITERATIONS, CLOCK_MONOTONIC);
    uint64_t time_gettime_cpu = safe_measure_time(test_gettimeofday_fast, FAST_ITERATIONS, CLOCK_PROCESS_CPUTIME_ID);

    uint64_t cycles_clockget = safe_measure_rdtsc(test_clock_gettime_fast, FAST_ITERATIONS);
    uint64_t time_clockget_mono = safe_measure_time(test_clock_gettime_fast, FAST_ITERATIONS, CLOCK_MONOTONIC);
    uint64_t time_clockget_cpu = safe_measure_time(test_clock_gettime_fast, FAST_ITERATIONS, CLOCK_PROCESS_CPUTIME_ID);

    uint64_t cycles_open = safe_measure_rdtsc(test_open_close_slow, SLOW_ITERATIONS);
    uint64_t time_open_mono = safe_measure_time(test_open_close_slow, SLOW_ITERATIONS, CLOCK_MONOTONIC);
    uint64_t time_open_cpu = safe_measure_time(test_open_close_slow, SLOW_ITERATIONS, CLOCK_PROCESS_CPUTIME_ID);

    printf("\nРезультаты (среднее время за операцию):\n");
    printf("=======================================\n");
    printf("| Операция           | Циклы CPU | Время (ns) MONO | Время (ns) CPU | Относ. MONO |\n");
    printf("|--------------------|------------|-----------------|----------------|-------------|\n");
    printf("| dummy()            | %8lu | %15lu | %14lu | %11.1fx |\n",
           cycles_dummy, time_dummy_mono, time_dummy_cpu, 1.0);
    printf("| getpid()           | %8lu | %15lu | %14lu | %11.1fx |\n",
           cycles_getpid, time_getpid_mono, time_getpid_cpu,
           (double)time_getpid_mono / time_dummy_mono);
    printf("| gettimeofday vDSO  | %8lu | %15lu | %14lu | %11.1fx |\n",
           cycles_gettime, time_gettime_mono, time_gettime_cpu,
           (double)time_gettime_mono / time_dummy_mono);
    printf("| clock_gettime vDSO | %8lu | %15lu | %14lu | %11.1fx |\n",
           cycles_clockget, time_clockget_mono, time_clockget_cpu,
           (double)time_clockget_mono / time_dummy_mono);
    printf("| open+close         | %8lu | %15lu | %14lu | %11.1fx |\n",
           cycles_open, time_open_mono, time_open_cpu,
           (double)time_open_mono / time_dummy_mono);

    printf("\nПримечания:\n");
    printf("- MONO: CLOCK_MONOTONIC (реальное время, включает ожидание I/O)\n");
    printf("- CPU:  CLOCK_PROCESS_CPUTIME_ID (только CPU время процесса)\n");
    printf("- Относительная скорость: во сколько раз медленнее userspace функции\n");
    printf("- Измерения проводятся 3 раза, выбирается наилучший результат\n");

    printf("\nРекомендации:\n");
    printf("- Запускайте бенчмарк на ненагруженной системе\n");
    printf("- Для точных результатов отключите частотные регулирования CPU:\n");
    printf("  sudo cpupower frequency-set --governor performance\n");

    return 0;
}