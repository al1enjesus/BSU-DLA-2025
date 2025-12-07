#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <x86intrin.h>
#include <time.h>
#include <stdint.h> 

// Пустая функция для сравнения
__attribute__((noinline)) int dummy() {
    return 42;
}

// Измерение времени в тактах
uint64_t measure_cycles(void (*func)(), int iterations) {
    uint64_t start = __rdtsc();
    func(iterations);
    uint64_t end = __rdtsc();
    return end - start;
}

// Измерение времени в наносекундах
long measure_ns(void (*func)(), int iterations) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    func(iterations);
    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
}

// Тестовые функции
void test_dummy(int iterations) {
    volatile int result;
    for (int i = 0; i < iterations; i++) {
        result = dummy();
    }
    (void)result;
}

void test_getpid(int iterations) {
    volatile pid_t result;
    for (int i = 0; i < iterations; i++) {
        result = getpid();
    }
    (void)result;
}

void test_open_close(int iterations) {
    for (int i = 0; i < iterations; i++) {
        int fd = open("/tmp/benchmark_test", O_RDWR | O_CREAT, 0644);
        if (fd != -1) {
            close(fd);
        }
    }
    remove("/tmp/benchmark_test");
}

void test_gettimeofday_vdso(int iterations) {
    struct timeval tv;
    for (int i = 0; i < iterations; i++) {
        gettimeofday(&tv, NULL);
    }
}

void test_read(int iterations) {
    char buffer[100];
    int fd = open("/dev/zero", O_RDONLY);
    if (fd == -1) {
        perror("open /dev/zero");
        return;
    }
    
    for (int i = 0; i < iterations; i++) {
        read(fd, buffer, 10);
    }
    close(fd);
}

void test_write(int iterations) {
    char buffer[] = "test";
    int fd = open("/dev/null", O_WRONLY);
    if (fd == -1) {
        perror("open /dev/null");
        return;
    }
    
    for (int i = 0; i < iterations; i++) {
        write(fd, buffer, 5);
    }
    close(fd);
}

int main() {
    const int iterations = 1000000;
    const int file_iterations = 100000; // Меньше для файловых операций
    
    printf("=== System Call Benchmark ===\n\n");
    printf("Iterations: %d (file operations: %d)\n\n", iterations, file_iterations);
    
    // Warmup
    test_dummy(1000);
    
    // Измерение dummy()
    long ns_dummy = measure_ns(test_dummy, iterations);
    uint64_t cycles_dummy = measure_cycles(test_dummy, iterations);
    
    // Измерение getpid()
    long ns_getpid = measure_ns(test_getpid, iterations);
    uint64_t cycles_getpid = measure_cycles(test_getpid, iterations);
    
    // Измерение open+close
    long ns_open_close = measure_ns(test_open_close, file_iterations);
    uint64_t cycles_open_close = measure_cycles(test_open_close, file_iterations);
    
    // Масштабируем до 1M итераций
    ns_open_close = (ns_open_close * iterations) / file_iterations;
    cycles_open_close = (cycles_open_close * iterations) / file_iterations;
    
    // Измерение gettimeofday()
    long ns_gettimeofday = measure_ns(test_gettimeofday_vdso, iterations);
    uint64_t cycles_gettimeofday = measure_cycles(test_gettimeofday_vdso, iterations);
    
    // Измерение read
    long ns_read = measure_ns(test_read, iterations);
    uint64_t cycles_read = measure_cycles(test_read, iterations);
    
    // Измерение write
    long ns_write = measure_ns(test_write, iterations);
    uint64_t cycles_write = measure_cycles(test_write, iterations);
    
    // Вывод результатов
    printf("| Operation              | Time (ns) | CPU Cycles | Slowdown vs userspace |\n");
    printf("|------------------------|-----------|------------|-----------------------|\n");
    printf("| dummy() userspace      | %7.1f  | %9.1f | 1.0x                  |\n", 
           (double)ns_dummy / iterations, (double)cycles_dummy / iterations);
    printf("| getpid()               | %7.1f  | %9.1f | %5.1fx                |\n", 
           (double)ns_getpid / iterations, (double)cycles_getpid / iterations, 
           (double)ns_getpid / ns_dummy);
    printf("| open()+close()         | %7.1f  | %9.1f | %5.1fx                |\n", 
           (double)ns_open_close / iterations, (double)cycles_open_close / iterations,
           (double)ns_open_close / ns_dummy);
    printf("| gettimeofday() vDSO    | %7.1f  | %9.1f | %5.1fx                |\n", 
           (double)ns_gettimeofday / iterations, (double)cycles_gettimeofday / iterations,
           (double)ns_gettimeofday / ns_dummy);
    printf("| read()                 | %7.1f  | %9.1f | %5.1fx                |\n",
           (double)ns_read / iterations, (double)cycles_read / iterations,
           (double)ns_read / ns_dummy);
    printf("| write()                | %7.1f  | %9.1f | %5.1fx                |\n",
           (double)ns_write / iterations, (double)cycles_write / iterations,
           (double)ns_write / ns_dummy);
    
    printf("\n=== Additional Info ===\n");
    printf("CPU Frequency: ~%.1f GHz\n", (double)cycles_dummy / ns_dummy);
    printf("Test file: /tmp/benchmark_test\n");
    printf("Special devices: /dev/zero, /dev/null\n");
    
    return 0;
}