// benchmark.c — измеряет cycles для dummy/getpid/open+close/gettimeofday(vDSO)
// Собирается с -O2
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
    uint64_t r = __rdtscp(&aux);
    return r;
}

__attribute__((noinline)) int dummy(void) {
    volatile int x = 0;
    x += 42;
    return x;
}

int main(void) {
    const int iters = 1000000; // 1 million
    int i;
    uint64_t start, end;
    uint64_t total;

    // 1) dummy()
    total = 0;
    for (i = 0; i < iters; ++i) {
        start = rdtsc();
        dummy();
        end = rdtsc();
        total += (end - start);
    }
    printf("dummy cycles avg: %.2f\n", (double)total / iters);

    // 2) getpid()
    total = 0;
    for (i = 0; i < iters; ++i) {
        start = rdtsc();
        (void)getpid();
        end = rdtsc();
        total += (end - start);
    }
    printf("getpid cycles avg: %.2f\n", (double)total / iters);

    // 3) open+close (file must exist)
    const char *fname = "/tmp/benchmark_scratch_file";
    // create file
    int fd = open(fname, O_CREAT|O_WRONLY, 0644);
    if (fd >= 0) { write(fd, "x", 1); close(fd); }

    total = 0;
    for (i = 0; i < iters; ++i) {
        start = rdtsc();
        int fd2 = open(fname, O_RDONLY);
        if (fd2 >= 0) close(fd2);
        end = rdtsc();
        total += (end - start);
    }
    printf("open+close cycles avg: %.2f\n", (double)total / iters);

    // 4) gettimeofday (vDSO)
    struct timeval tv;
    total = 0;
    for (i = 0; i < iters; ++i) {
        start = rdtsc();
        gettimeofday(&tv, NULL);
        end = rdtsc();
        total += (end - start);
    }
    printf("gettimeofday cycles avg: %.2f\n", (double)total / iters);

    // print rough ns values assuming 3.0 GHz
    double cpu_ghz = 3.0;
    printf("\nAssuming CPU = %.2f GHz (1 cycle = %.3f ns)\n", cpu_ghz, 1.0/(cpu_ghz));
    return 0;
}
