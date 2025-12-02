#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <x86intrin.h>
#include <stdlib.h>

// Глобальная переменная для предотвращения оптимизации
volatile int sink = 0;

// Получение времени в наносекундах с монотонными часами
static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Простая функция-заглушка
static int dummy_func(void) {
    return 42;
}

// Универсальный цикл прогрева
static void warmup(int (*fn)(void), int count) {
    for (int i = 0; i < count; ++i) {
        sink += fn();
    }
}

// Измерение в циклах TSC
static uint64_t measure_cycles(int (*fn)(void), int iterations) {
    uint64_t start = __rdtsc();
    for (int i = 0; i < iterations; ++i) {
        sink += fn();
    }
    uint64_t end = __rdtsc();
    return end - start;
}

// Измерение в наносекундах
static uint64_t measure_time_ns(void (*fn)(void), int iterations) {
    uint64_t start = now_ns();
    for (int i = 0; i < iterations; ++i) {
        fn();
    }
    return now_ns() - start;
}

// Обёртки для измерения системных вызовов
static void call_getpid(void) {
    (void)getpid();
}

static void call_clock_gettime_realtime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    (void)ts.tv_sec;
}

static void call_open_close(void) {
    int fd = open("/tmp/benchmark_testfile", O_RDONLY);
    if (fd >= 0) close(fd);
}

int main(int argc, char **argv) {
    const int default_iters = 1000000;
    int iters = (argc > 1) ? atoi(argv[1]) : default_iters;

    printf("Iters=%d\n", iters);

    // Прогрев
    warmup(dummy_func, 1000);

    // Измерения
    uint64_t cycles_dummy = measure_cycles(dummy_func, iters);
    uint64_t ns_dummy = measure_time_ns((void (*)(void))dummy_func, iters);
    uint64_t ns_getpid = measure_time_ns(call_getpid, iters);
    uint64_t ns_gettime = measure_time_ns(call_clock_gettime_realtime, iters);

    // Подготовка файла для open/close
    const char *testfile = "/tmp/benchmark_testfile";
    int fd = open(testfile, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        write(fd, "test\n", 5);
        close(fd);
    }

    const int open_iters = 100000;
    uint64_t ns_open_total = measure_time_ns(call_open_close, open_iters);
    double ns_open_avg = (double)ns_open_total / open_iters;

    // Вывод результатов
    printf("\nRESULTS (averages):\n");
    printf("dummy()         : %.3f ns, %.3f cycles\n",
           (double)ns_dummy / iters,
           (double)cycles_dummy / iters);
    printf("getpid()        : %.3f ns\n", (double)ns_getpid / iters);
    printf("gettimeofday()  : %.3f ns\n", (double)ns_gettime / iters);
    printf("open()+close()  : %.3f ns (measured over %d iterations)\n",
           ns_open_avg, open_iters);

    printf("\nRaw cycles (dummy total): %llu\n", (unsigned long long)cycles_dummy);
    printf("Sink: %d\n", sink);

    return 0;
}