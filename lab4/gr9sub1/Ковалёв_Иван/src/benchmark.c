#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <x86intrin.h>

// Userspace функция
int dummy_function() {
    return 42;
}

// Измерение времени в наносекундах
uint64_t get_nanoseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main() {
    const int iterations = 1000000;
    uint64_t total_time, total_cycles;
    
    printf("=== System Call Benchmark ===\n");
    printf("Iterations: %d\n\n", iterations);
    
    // 1. Userspace функция
    total_time = 0;
    total_cycles = 0;
    for (int i = 0; i < iterations; i++) {
        uint64_t start_time = get_nanoseconds();
        uint64_t start_cycles = __rdtsc();
        
        dummy_function();
        
        uint64_t end_cycles = __rdtsc();
        uint64_t end_time = get_nanoseconds();
        
        total_cycles += (end_cycles - start_cycles);
        total_time += (end_time - start_time);
    }
    printf("dummy() userspace: %lu ns, %lu cycles\n", total_time / iterations, total_cycles / iterations);
    
    // 2. getpid()
    total_time = 0;
    total_cycles = 0;
    for (int i = 0; i < iterations; i++) {
        uint64_t start_time = get_nanoseconds();
        uint64_t start_cycles = __rdtsc();
        
        getpid();
        
        uint64_t end_cycles = __rdtsc();
        uint64_t end_time = get_nanoseconds();
        
        total_cycles += (end_cycles - start_cycles);
        total_time += (end_time - start_time);
    }
    printf("getpid() syscall: %lu ns, %lu cycles\n", total_time / iterations, total_cycles / iterations);
    
    // 3. open()+close()
    system("echo 'test' > /tmp/benchmark_file");
    total_time = 0;
    total_cycles = 0;
    for (int i = 0; i < iterations; i++) {
        uint64_t start_time = get_nanoseconds();
        uint64_t start_cycles = __rdtsc();
        
        int fd = open("/tmp/benchmark_file", O_RDONLY);
        if (fd != -1) close(fd);
        
        uint64_t end_cycles = __rdtsc();
        uint64_t end_time = get_nanoseconds();
        
        total_cycles += (end_cycles - start_cycles);
        total_time += (end_time - start_time);
    }
    system("rm -f /tmp/benchmark_file");
    printf("open()+close(): %lu ns, %lu cycles\n", total_time / iterations, total_cycles / iterations);
    
    // 4. gettimeofday() vDSO
    struct timeval tv;
    total_time = 0;
    total_cycles = 0;
    for (int i = 0; i < iterations; i++) {
        uint64_t start_time = get_nanoseconds();
        uint64_t start_cycles = __rdtsc();
        
        gettimeofday(&tv, NULL);
        
        uint64_t end_cycles = __rdtsc();
        uint64_t end_time = get_nanoseconds();
        
        total_cycles += (end_cycles - start_cycles);
        total_time += (end_time - start_time);
    }
    printf("gettimeofday() vDSO: %lu ns, %lu cycles\n", total_time / iterations, total_cycles / iterations);
    
    return 0;
}