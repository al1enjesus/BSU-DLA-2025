#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <x86intrin.h>
#include <string.h>
#include <time.h>
#include <stdint.h>  // ДОБАВИТЬ ЭТУ СТРОКУ!

#define ITERATIONS 1000000

// Пустая функция для сравнения
int dummy_function() {
    return 42;
}

// Измерение тактов процессора
uint64_t measure_cycles(void (*func)(void)) {
    uint64_t start = __rdtsc();
    func();
    uint64_t end = __rdtsc();
    return end - start;
}

// Измерение наносекунд через clock_gettime
long measure_nanoseconds(void (*func)(void)) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    func();
    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
}

// Тестовые функции
void test_dummy() {
    volatile int result = dummy_function();
    (void)result;
}

void test_getpid() {
    volatile pid_t result = getpid();
    (void)result;
}

void test_gettimeofday() {
    struct timeval tv;
    volatile int result = gettimeofday(&tv, NULL);
    (void)result;
}

void test_open_close() {
    int fd = open("/tmp/benchmark_test", O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        close(fd);
    }
}

// Функция для запуска бенчмарка
void run_benchmark(const char* name, void (*test_func)(void)) {
    printf("Benchmarking: %s\n", name);
    
    // Прогрев
    for (int i = 0; i < 1000; i++) {
        test_func();
    }
    
    // Измерение тактов
    uint64_t total_cycles = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        total_cycles += measure_cycles(test_func);
    }
    double avg_cycles = (double)total_cycles / ITERATIONS;
    

    long total_ns = 0;
    for (int i = 0; i < ITERATIONS / 100; i++) { // Меньше итераций для времени
        total_ns += measure_nanoseconds(test_func);
    }
    double avg_ns = (double)total_ns / (ITERATIONS / 100);
    
    printf("  Average cycles: %.2f\n", avg_cycles);
    printf("  Average time: %.2f ns\n", avg_ns);
    printf("  Slowdown vs dummy: %.2fx\n\n", avg_ns / 2.0); // dummy ~2ns
}

// Эксперимент с кэшем
void run_cache_experiment() {
    printf("=== Cache Experiment ===\n");
    
    // Создаем тестовый файл
    system("echo 'test content' > /tmp/cache_test");
    
    // Холодный кэш
    system("sync; echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null");
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int fd = open("/tmp/cache_test", O_RDONLY);
    if (fd != -1) close(fd);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    long cold_time = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
    printf("Cold cache open+close: %ld ns\n", cold_time);
    
    // Горячий кэш
    clock_gettime(CLOCK_MONOTONIC, &start);
    fd = open("/tmp/cache_test", O_RDONLY);
    if (fd != -1) close(fd);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    long hot_time = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
    printf("Hot cache open+close: %ld ns\n", hot_time);
    printf("Difference: %.2fx\n\n", (double)cold_time / hot_time);
}

int main() {
    printf("=== System Call Benchmark ===\n");
    printf("Iterations: %d\n\n", ITERATIONS);
    
    // Создаем тестовый файл
    system("touch /tmp/benchmark_test");
    
    run_benchmark("dummy() - userspace function", test_dummy);
    run_benchmark("getpid() - fast syscall", test_getpid);
    run_benchmark("gettimeofday() - vDSO call", test_gettimeofday);
    run_benchmark("open()+close() - file operation", test_open_close);
    
    run_cache_experiment();
    
    // Убираем временные файлы
    system("rm -f /tmp/benchmark_test /tmp/cache_test");
    
    return 0;
}