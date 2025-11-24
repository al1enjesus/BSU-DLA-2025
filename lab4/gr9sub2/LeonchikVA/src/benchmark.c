#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define ITERATIONS 1000000
#define FILENAME "/tmp/benchmark_test_file.txt"

// Чтение счетчика циклов (RDTSC)
static inline unsigned long long rdtsc_start(void) {
    unsigned int lo, hi;
    // Используем rdtsc, чтобы получить как можно более точный счетчик циклов CPU
    asm volatile ("CPUID\n\t"
                  "RDTSC\n\t"
                  "mov %%edx, %0\n\t"
                  "mov %%eax, %1\n\t"
                  : "=r" (hi), "=r" (lo)
                  :: "%eax", "%ebx", "%ecx", "%edx");
    return ((unsigned long long)hi << 32) | lo;
}

static inline unsigned long long rdtsc_end(void) {
    unsigned int lo, hi;
    // Используем rdtscp для завершения, чтобы убедиться, что все предыдущие инструкции выполнены
    asm volatile ("RDTSCP\n\t"
                  "mov %%edx, %0\n\t"
                  "mov %%eax, %1\n\t"
                  "CPUID\n\t"
                  : "=r" (hi), "=r" (lo)
                  :: "%eax", "%ebx", "%ecx", "%edx");
    return ((unsigned long long)hi << 32) | lo;
}

// 1. Userspace baseline
void dummy() {
    // Пустая функция для измерения минимальных накладных расходов вызова функции
}

void run_benchmark(const char *name, void (*func)(void)) {
    unsigned long long total_cycles = 0;
    struct timespec start, end;
    long total_ns = 0;

    // Разогрев (Warmup)
    for (int i = 0; i < 1000; i++) {
        func();
    }

    // Измерение
    unsigned long long c_start = rdtsc_start();
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        func();
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    unsigned long long c_end = rdtsc_end();
    total_cycles = c_end - c_start;

    // Расчет времени
    total_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);

    printf("%-25s | %10.2f | %10llu\n", 
           name, 
           (double)total_ns / ITERATIONS, 
           total_cycles / ITERATIONS);
}

void getpid_wrapper() {
    getpid();
}

void gettimeofday_wrapper() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
}

void open_close_wrapper() {
    int fd = open(FILENAME, O_RDWR | O_CREAT, 0666);
    if (fd >= 0) {
        close(fd);
    }
}

int main() {
    // Создаем файл для open/close
    int fd = open(FILENAME, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("Error creating test file");
        return 1;
    }
    close(fd);

    printf("╔════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                 Benchmark (1,000,000 итераций)                 ║\n");
    printf("╠═══════════════════════════╤════════════╤═══════════════════════╣\n");
    printf("║ Операция                | Ср. время (ns) | Ср. циклов CPU/вызов  ║\n");
    printf("╠═══════════════════════════╧════════════╧═══════════════════════╣\n");
    printf("║ Baseline (Userspace):                                         ║\n");
    run_benchmark("dummy() userspace", dummy);
    printf("╠═══════════════════════════╧════════════╧═══════════════════════╣\n");
    printf("║ Syscalls (Kernelspace):                                       ║\n");
    run_benchmark("getpid() syscall", getpid_wrapper);
    run_benchmark("gettimeofday() vDSO", gettimeofday_wrapper);
    printf("║ I/O Syscalls (Kernelspace + FS Overhead):                     ║\n");
    run_benchmark("open/close() (I/O)", open_close_wrapper);
    printf("╚════════════════════════════════════════════════════════════════════════╝\n");

    // Удаляем файл
    unlink(FILENAME);
    return 0;
}