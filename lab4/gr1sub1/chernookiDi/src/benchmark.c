#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>     // для uint64_t
#include <x86intrin.h>  // для __rdtsc()

#define ITERATIONS 1000000

// Простая функция userspace (baseline)
__attribute__((noinline))
int dummy() {
    volatile int x = 42;
    return x;
}

// Функция для измерения времени в наносекундах
static inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

// Функция для измерения циклов CPU
static inline uint64_t get_cycles() {
    return __rdtsc();
}

void benchmark_function(const char *name, void (*func)()) {
    printf("\n=== Бенчмарк: %s ===\n", name);
    
    // Warmup
    for (int i = 0; i < 1000; i++) {
        func();
    }
    
    // Измерение времени
    uint64_t start_ns = get_time_ns();
    uint64_t start_cycles = get_cycles();
    
    for (int i = 0; i < ITERATIONS; i++) {
        func();
    }
    
    uint64_t end_ns = get_time_ns();
    uint64_t end_cycles = get_cycles();
    
    uint64_t total_ns = end_ns - start_ns;
    uint64_t total_cycles = end_cycles - start_cycles;
    
    double avg_ns = (double)total_ns / ITERATIONS;
    double avg_cycles = (double)total_cycles / ITERATIONS;
    
    printf("Всего итераций: %d\n", ITERATIONS);
    printf("Общее время: %lu нс\n", (unsigned long)total_ns);
    printf("Среднее время: %.2f нс\n", avg_ns);
    printf("Средние циклы: %.2f\n", avg_cycles);
}

void test_dummy() {
    dummy();
}

void test_getpid() {
    getpid();
}

void test_open_close() {
    int fd = open("/tmp/benchmark_test", O_CREAT | O_RDWR, 0644);
    if (fd >= 0) {
        close(fd);
    }
}

void test_gettimeofday() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
}

int main() {
    printf("Benchmark системных вызовов\n");
    printf("Количество итераций: %d\n", ITERATIONS);
    
    // Создаём тестовый файл
    int fd = open("/tmp/benchmark_test", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        if (write(fd, "test", 4) == -1) {
            perror("write");
        }
        close(fd);
    }
    
    // Базовая линия - функция userspace
    double baseline_ns, baseline_cycles;
    {
        uint64_t start_ns = get_time_ns();
        uint64_t start_cycles = get_cycles();
        
        for (int i = 0; i < ITERATIONS; i++) {
            dummy();
        }
        
        uint64_t end_ns = get_time_ns();
        uint64_t end_cycles = get_cycles();
        
        baseline_ns = (double)(end_ns - start_ns) / ITERATIONS;
        baseline_cycles = (double)(end_cycles - start_cycles) / ITERATIONS;
        
        printf("\n=== BASELINE: dummy() ===\n");
        printf("Среднее время: %.2f нс\n", baseline_ns);
        printf("Средние циклы: %.2f\n", baseline_cycles);
    }
    
    // Быстрый syscall
    {
        uint64_t start_ns = get_time_ns();
        uint64_t start_cycles = get_cycles();
        
        for (int i = 0; i < ITERATIONS; i++) {
            getpid();
        }
        
        uint64_t end_ns = get_time_ns();
        uint64_t end_cycles = get_cycles();
        
        double avg_ns = (double)(end_ns - start_ns) / ITERATIONS;
        double avg_cycles = (double)(end_cycles - start_cycles) / ITERATIONS;
        double slowdown = avg_ns / baseline_ns;
        
        printf("\n=== SYSCALL: getpid() ===\n");
        printf("Среднее время: %.2f нс\n", avg_ns);
        printf("Средние циклы: %.2f\n", avg_cycles);
        printf("Замедление: %.1fx\n", slowdown);
    }
    
    // Медленный syscall (файловые операции)
    {
        uint64_t start_ns = get_time_ns();
        uint64_t start_cycles = get_cycles();
        
        for (int i = 0; i < ITERATIONS; i++) {
            int fd = open("/tmp/benchmark_test", O_RDONLY);
            if (fd >= 0) {
                close(fd);
            }
        }
        
        uint64_t end_ns = get_time_ns();
        uint64_t end_cycles = get_cycles();
        
        double avg_ns = (double)(end_ns - start_ns) / ITERATIONS;
        double avg_cycles = (double)(end_cycles - start_cycles) / ITERATIONS;
        double slowdown = avg_ns / baseline_ns;
        
        printf("\n=== SYSCALL: open+close ===\n");
        printf("Среднее время: %.2f нс\n", avg_ns);
        printf("Средние циклы: %.2f\n", avg_cycles);
        printf("Замедление: %.1fx\n", slowdown);
    }
    
    // vDSO вызов
    {
        uint64_t start_ns = get_time_ns();
        uint64_t start_cycles = get_cycles();
        
        for (int i = 0; i < ITERATIONS; i++) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
        }
        
        uint64_t end_ns = get_time_ns();
        uint64_t end_cycles = get_cycles();
        
        double avg_ns = (double)(end_ns - start_ns) / ITERATIONS;
        double avg_cycles = (double)(end_cycles - start_cycles) / ITERATIONS;
        double slowdown = avg_ns / baseline_ns;
        
        printf("\n=== vDSO: gettimeofday() ===\n");
        printf("Среднее время: %.2f нс\n", avg_ns);
        printf("Средние циклы: %.2f\n", avg_cycles);
        printf("Замедление: %.1fx\n", slowdown);
    }
    
    // Итоговая таблица
    printf("\n=== ИТОГОВАЯ ТАБЛИЦА ===\n");
    printf("| Операция           | Время (ns) | Циклов CPU | Замедление |\n");
    printf("|--------------------|------------|------------|------------|\n");
    printf("| dummy() userspace  | %.2f       | %.2f       | 1.0x       |\n", baseline_ns, baseline_cycles);
    
    // Очистка
    unlink("/tmp/benchmark_test");
    
    return 0;
}