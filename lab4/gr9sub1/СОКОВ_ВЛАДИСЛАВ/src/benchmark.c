#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <x86intrin.h>

static inline uint64_t rdtsc_begin(void) {
    unsigned int aux;
    uint64_t r = __rdtscp(&aux);
    return r;
}
static inline uint64_t rdtsc_end(void) {
    unsigned int aux;
    uint64_t r = __rdtscp(&aux);
    return r;
}

__attribute__((noinline)) volatile int dummy(void) { return 42; }

int main() {
    const long N = 1000000; // 1e6
    uint64_t start, end;
    uint64_t sum_cycles = 0;

    // Warmup
    for (int i=0;i<1000;i++) dummy();

    // 1) userspace dummy
    sum_cycles = 0;
    for (long i=0;i<N;i++) {
        start = rdtsc_begin();
        volatile int r = dummy(); (void)r;
        end = rdtsc_end();
        sum_cycles += (end - start);
    }
    printf("dummy: total_cycles=%lu avg_cycles=%.2f\n", (unsigned long)sum_cycles, (double)sum_cycles / N);

    // 2) getpid (syscall but cached)
    sum_cycles = 0;
    for (long i=0;i<N;i++) {
        start = rdtsc_begin();
        pid_t p = getpid(); (void)p;
        end = rdtsc_end();
        sum_cycles += (end - start);
    }
    printf("getpid: total_cycles=%lu avg_cycles=%.2f\n", (unsigned long)sum_cycles, (double)sum_cycles / N);

    // 3) gettimeofday (vDSO on modern Linux)
    sum_cycles = 0;
    struct timeval tv;
    for (long i=0;i<N;i++) {
        start = rdtsc_begin();
        gettimeofday(&tv, NULL);
        end = rdtsc_end();
        sum_cycles += (end - start);
    }
    printf("gettimeofday: total_cycles=%lu avg_cycles=%.2f\n", (unsigned long)sum_cycles, (double)sum_cycles / N);

    // 4) open+close on /tmp/testfile
    // подготовим файл
    const char *fpath = "/tmp/lab4_bench_testfile";
    int tf = open(fpath, O_CREAT|O_WRONLY, 0644);
    if (tf>=0) { write(tf, "hello", 5); close(tf); }

    sum_cycles = 0;
    for (long i=0;i<N;i++) {
        start = rdtsc_begin();
        int fd = open(fpath, O_RDONLY);
        if (fd>=0) close(fd);
        end = rdtsc_end();
        sum_cycles += (end - start);
    }
    printf("open+close: total_cycles=%lu avg_cycles=%.2f\n", (unsigned long)sum_cycles, (double)sum_cycles / N);

    // print note
    printf("Iterations: %ld\n", N);
    return 0;
}