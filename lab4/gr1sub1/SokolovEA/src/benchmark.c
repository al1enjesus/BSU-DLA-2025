#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include <x86intrin.h>

#define ITERATIONS 1000000

// Dummy функция для измерения базового времени
__attribute__((noinline))
int dummy_function() {
    return 42;
}

// Измерение времени в наносекундах
uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Измерение циклов CPU через RDTSC
uint64_t get_cycles() {
    return __rdtsc();
}

// Бенчмарк userspace функции
void benchmark_userspace() {
    printf("=== Бенчмарк userspace функции ===\n");
    
    uint64_t start_time = get_time_ns();
    uint64_t start_cycles = get_cycles();
    
    volatile int result;
    for (int i = 0; i < ITERATIONS; i++) {
        result = dummy_function();
    }
    
    uint64_t end_time = get_time_ns();
    uint64_t end_cycles = get_cycles();
    
    uint64_t total_time = end_time - start_time;
    uint64_t total_cycles = end_cycles - start_cycles;
    
    printf("Userspace dummy(): %lu итераций\n", ITERATIONS);
    printf("Общее время: %lu ns\n", total_time);
    printf("Среднее время: %.2f ns на вызов\n", (double)total_time / ITERATIONS);
    printf("Общие циклы: %lu\n", total_cycles);
    printf("Средние циклы: %.2f на вызов\n", (double)total_cycles / ITERATIONS);
    printf("\n");
}

// Бенчмарк быстрого системного вызова
void benchmark_getpid() {
    printf("=== Бенчмарк getpid() ===\n");
    
    uint64_t start_time = get_time_ns();
    uint64_t start_cycles = get_cycles();
    
    volatile pid_t result;
    for (int i = 0; i < ITERATIONS; i++) {
        result = getpid();
    }
    
    uint64_t end_time = get_time_ns();
    uint64_t end_cycles = get_cycles();
    
    uint64_t total_time = end_time - start_time;
    uint64_t total_cycles = end_cycles - start_cycles;
    
    printf("getpid(): %lu итераций\n", ITERATIONS);
    printf("Общее время: %lu ns\n", total_time);
    printf("Среднее время: %.2f ns на вызов\n", (double)total_time / ITERATIONS);
    printf("Общие циклы: %lu\n", total_cycles);
    printf("Средние циклы: %.2f на вызов\n", (double)total_cycles / ITERATIONS);
    printf("\n");
}

// Бенчмарк медленного системного вызова (открытие файла)
void benchmark_open_close() {
    printf("=== Бенчмарк open() + close() ===\n");
    
    // Создаём тестовый файл
    int test_fd = open("/tmp/benchmark_test", O_CREAT | O_WRONLY, 0644);
    if (test_fd >= 0) {
        write(test_fd, "test", 4);
        close(test_fd);
    }
    
    uint64_t start_time = get_time_ns();
    uint64_t start_cycles = get_cycles();
    
    for (int i = 0; i < ITERATIONS; i++) {
        int fd = open("/tmp/benchmark_test", O_RDONLY);
        if (fd >= 0) {
            close(fd);
        }
    }
    
    uint64_t end_time = get_time_ns();
    uint64_t end_cycles = get_cycles();
    
    uint64_t total_time = end_time - start_time;
    uint64_t total_cycles = end_cycles - start_cycles;
    
    printf("open() + close(): %lu итераций\n", ITERATIONS);
    printf("Общее время: %lu ns\n", total_time);
    printf("Среднее время: %.2f ns на вызов\n", (double)total_time / ITERATIONS);
    printf("Общие циклы: %lu\n", total_cycles);
    printf("Средние циклы: %.2f на вызов\n", (double)total_cycles / ITERATIONS);
    printf("\n");
    
    // Удаляем тестовый файл
    unlink("/tmp/benchmark_test");
}

// Бенчмарк vDSO вызова
void benchmark_gettimeofday() {
    printf("=== Бенчмарк gettimeofday() (vDSO) ===\n");
    
    uint64_t start_time = get_time_ns();
    uint64_t start_cycles = get_cycles();
    
    struct timeval tv;
    for (int i = 0; i < ITERATIONS; i++) {
        gettimeofday(&tv, NULL);
    }
    
    uint64_t end_time = get_time_ns();
    uint64_t end_cycles = get_cycles();
    
    uint64_t total_time = end_time - start_time;
    uint64_t total_cycles = end_cycles - start_cycles;
    
    printf("gettimeofday(): %lu итераций\n", ITERATIONS);
    printf("Общее время: %lu ns\n", total_time);
    printf("Среднее время: %.2f ns на вызов\n", (double)total_time / ITERATIONS);
    printf("Общие циклы: %lu\n", total_cycles);
    printf("Средние циклы: %.2f на вызов\n", (double)total_cycles / ITERATIONS);
    printf("\n");
}

int main() {
    printf("Бенчмарк системных вызовов\n");
    printf("Количество итераций: %d\n", ITERATIONS);
    printf("==========================================\n\n");
    
    // Warmup
    for (int i = 0; i < 1000; i++) {
        dummy_function();
        getpid();
    }
    
    benchmark_userspace();
    benchmark_getpid();
    benchmark_open_close();
    benchmark_gettimeofday();
    
    printf("=== Сравнительная таблица ===\n");
    printf("Операция                | Время (ns) | Циклов CPU | Замедление\n");
    printf("------------------------|------------|------------|----------\n");
    printf("dummy() userspace       | baseline   | baseline   | 1x\n");
    printf("getpid()                | измерено   | измерено   | Xx\n");
    printf("open() + close()        | измерено   | измерено   | Yx\n");
    printf("gettimeofday() vDSO     | измерено   | измерено   | Zx\n");
    
    return 0;
}