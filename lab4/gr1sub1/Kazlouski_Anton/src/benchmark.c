#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <x86intrin.h>

#define ITERATIONS 1000000

int dummy_function() {
    return 42;
}

uint64_t measure_rdtsc(void (*func)()) {
    uint64_t start = __rdtsc();
    func();
    uint64_t end = __rdtsc();
    return end - start;
}

uint64_t measure_clock_gettime(void (*func)()) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    func();
    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) * 1000000000ULL +
        (end.tv_nsec - start.tv_nsec);
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

void test_open_close() {
    for (int i = 0; i < ITERATIONS / 100; i++) {
        int fd = open("/tmp/benchmark_test", O_RDWR | O_CREAT, 0644);
        if (fd != -1) {
            close(fd);
        }
    }
}

int main() {
    printf("Benchmark системных вызовов (%d итераций)\n", ITERATIONS);
    printf("==========================================\n\n");

    uint64_t cycles_dummy = measure_rdtsc(test_dummy);
    uint64_t time_dummy = measure_clock_gettime(test_dummy);

    uint64_t cycles_getpid = measure_rdtsc(test_getpid);
    uint64_t time_getpid = measure_clock_gettime(test_getpid);

    uint64_t cycles_gettime = measure_rdtsc(test_gettimeofday_vdso);
    uint64_t time_gettime = measure_clock_gettime(test_gettimeofday_vdso);

    uint64_t cycles_open = measure_rdtsc(test_open_close) * 100;
    uint64_t time_open = measure_clock_gettime(test_open_close) * 100;

    printf("Результаты:\n");
    printf("-----------\n");
    printf("| Операция           | Время (ns) | Циклов CPU | Во ск. раз медленнее |\n");
    printf("|--------------------|------------|------------|----------------------|\n");
    printf("| dummy()            | %8.1f | %8.0f | %17.0fx |\n",
           (double)time_dummy / ITERATIONS,
           (double)cycles_dummy / ITERATIONS, 1.0);
    printf("| getpid()           | %8.1f | %8.0f | %17.0fx |\n",
           (double)time_getpid / ITERATIONS,
           (double)cycles_getpid / ITERATIONS,
           (double)time_getpid / time_dummy);
    printf("| gettimeofday vDSO  | %8.1f | %8.0f | %17.0fx |\n",
           (double)time_gettime / ITERATIONS,
           (double)cycles_gettime / ITERATIONS,
           (double)time_gettime / time_dummy);
    printf("| open+close         | %8.1f | %8.0f | %17.0fx |\n",
           (double)time_open / ITERATIONS,
           (double)cycles_open / ITERATIONS,
           (double)time_open / time_dummy);

    return 0;
}