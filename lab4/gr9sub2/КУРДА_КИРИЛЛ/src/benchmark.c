// benchmark.c — измеряет cycles для dummy/getpid/open+close/gettimeofday(vDSO)
// Компиляция: gcc -O2 -march=native -o benchmark benchmark.c

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <x86intrin.h>

static inline uint64_t rdtsc(void) {
    unsigned int aux;
    uint64_t r = __rdtscp(&aux);  // сериализующая версия RDTSC
    return r;
}

__attribute__((noinline))
int dummy(void) {
    volatile int x = 0;
    x += 42;
    return x;
}

int main(void) {
    const int iters = 1000000;  // 1 миллион итераций
    int i;
    uint64_t start, end;
    uint64_t total;
    
    printf("=== Benchmark системных вызовов ===\n");
    printf("Итераций: %d\n\n", iters);
    
    total = 0;
    for (i = 0; i < iters; ++i) {
        start = rdtsc();
        dummy();
        end = rdtsc();
        total += (end - start);
    }
    double dummy_avg = (double)total / iters;
    printf("dummy() userspace:     %8.2f cycles/call\n", dummy_avg);
    
    total = 0;
    for (i = 0; i < iters; ++i) {
        start = rdtsc();
        (void)getpid();
        end = rdtsc();
        total += (end - start);
    }
    double getpid_avg = (double)total / iters;
    printf("getpid() syscall:      %8.2f cycles/call  (%.1fx медленнее)\n", 
           getpid_avg, getpid_avg / dummy_avg);
    
    const char *fname = "/tmp/benchmark_scratch_file";
    
    // Создаём тестовый файл
    int fd = open(fname, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        write(fd, "x", 1);
        close(fd);
    }
    
    total = 0;
    for (i = 0; i < iters; ++i) {
        start = rdtsc();
        int fd2 = open(fname, O_RDONLY);
        if (fd2 >= 0) 
            close(fd2);
        end = rdtsc();
        total += (end - start);
    }
    double open_close_avg = (double)total / iters;
    printf("open+close syscall:    %8.2f cycles/call  (%.1fx медленнее)\n", 
           open_close_avg, open_close_avg / dummy_avg);
    
    struct timeval tv;
    total = 0;
    for (i = 0; i < iters; ++i) {
        start = rdtsc();
        gettimeofday(&tv, NULL);
        end = rdtsc();
        total += (end - start);
    }
    double gettimeofday_avg = (double)total / iters;
    printf("gettimeofday vDSO:     %8.2f cycles/call  (%.1fx медленнее)\n", 
           gettimeofday_avg, gettimeofday_avg / dummy_avg);
    
    printf("\n=== Время в наносекундах (примерно при 3.0 GHz CPU) ===\n");
    double cpu_ghz = 3.0;
    double ns_per_cycle = 1.0 / cpu_ghz;
    
    printf("dummy() userspace:     %8.2f ns\n", dummy_avg * ns_per_cycle);
    printf("getpid() syscall:      %8.2f ns\n", getpid_avg * ns_per_cycle);
    printf("open+close syscall:    %8.2f ns\n", open_close_avg * ns_per_cycle);
    printf("gettimeofday vDSO:     %8.2f ns\n", gettimeofday_avg * ns_per_cycle);
    
    printf("\nПримечание: 1 цикл CPU @ %.1f GHz = %.3f ns\n", cpu_ghz, ns_per_cycle);
    
    return 0;
}