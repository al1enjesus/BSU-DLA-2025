#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <x86intrin.h>

#define ITERATIONS 1000000
#define OPEN_ITERATIONS 1000

__attribute__((noinline)) int dummy_function() {
    return 42;
}

uint64_t measure_rdtsc(void (*func)(), int iterations) {
    uint64_t start = __rdtsc();
    func();
    uint64_t end = __rdtsc();
    return (end - start) / iterations;
}

uint64_t measure_clock_monotonic(void (*func)(), int iterations) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    func();
    clock_gettime(CLOCK_MONOTONIC, &end);

    uint64_t ns_start = start.tv_sec * 1000000000ULL + start.tv_nsec;
    uint64_t ns_end = end.tv_sec * 1000000000ULL + end.tv_nsec;
    return (ns_end - ns_start) / iterations;
}

uint64_t measure_clock_process_cputime(void (*func)(), int iterations) {
    struct timespec start, end;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
    func();
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);

    uint64_t ns_start = start.tv_sec * 1000000000ULL + start.tv_nsec;
    uint64_t ns_end = end.tv_sec * 1000000000ULL + end.tv_nsec;
    return (ns_end - ns_start) / iterations;
}

void test_dummy() {
    volatile int result;
    for (int i = 0; i < ITERATIONS; i++) {
        result = dummy_function();
    }
    (void)result;
}

void test_getpid() {
    volatile pid_t result;
    for (int i = 0; i < ITERATIONS; i++) {
        result = getpid();
    }
    (void)result;
}

void test_gettimeofday_vdso() {
    struct timeval tv;
    for (int i = 0; i < ITERATIONS; i++) {
        gettimeofday(&tv, NULL);
    }
}

void test_clock_gettime_vdso() {
    struct timespec ts;
    for (int i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
    }
}

void test_open_close() {
    const char* test_file = "/tmp/benchmark_test_file";
    for (int i = 0; i < OPEN_ITERATIONS; i++) {
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
    printf("- Быстрые операции: %d итераций\n", ITERATIONS);
    printf("- Медленные операции: %d итераций\n\n", OPEN_ITERATIONS);

    uint64_t cycles_dummy = measure_rdtsc(test_dummy, ITERATIONS);
    uint64_t time_dummy_mono = measure_clock_monotonic(test_dummy, ITERATIONS);
    uint64_t time_dummy_cpu = measure_clock_process_cputime(test_dummy, ITERATIONS);

    uint64_t cycles_getpid = measure_rdtsc(test_getpid, ITERATIONS);
    uint64_t time_getpid_mono = measure_clock_monotonic(test_getpid, ITERATIONS);
    uint64_t time_getpid_cpu = measure_clock_process_cputime(test_getpid, ITERATIONS);

    uint64_t cycles_gettime = measure_rdtsc(test_gettimeofday_vdso, ITERATIONS);
    uint64_t time_gettime_mono = measure_clock_monotonic(test_gettimeofday_vdso, ITERATIONS);
    uint64_t time_gettime_cpu = measure_clock_process_cputime(test_gettimeofday_vdso, ITERATIONS);

    uint64_t cycles_clockget = measure_rdtsc(test_clock_gettime_vdso, ITERATIONS);
    uint64_t time_clockget_mono = measure_clock_monotonic(test_clock_gettime_vdso, ITERATIONS);
    uint64_t time_clockget_cpu = measure_clock_process_cputime(test_clock_gettime_vdso, ITERATIONS);

    uint64_t cycles_open = measure_rdtsc(test_open_close, OPEN_ITERATIONS);
    uint64_t time_open_mono = measure_clock_monotonic(test_open_close, OPEN_ITERATIONS);
    uint64_t time_open_cpu = measure_clock_process_cputime(test_open_close, OPEN_ITERATIONS);

    printf("Результаты (среднее время за операцию):\n");
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
    printf("- MONO: CLOCK_MONOTONIC (реальное время)\n");
    printf("- CPU:  CLOCK_PROCESS_CPUTIME_ID (только CPU время процесса)\n");
    printf("- Относительная скорость: во сколько раз медленнее userspace функции\n");

    return 0;
}