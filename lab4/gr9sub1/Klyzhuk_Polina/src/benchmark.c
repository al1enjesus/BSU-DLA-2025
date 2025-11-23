#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/time.h>
#ifdef __x86_64__
#include <x86intrin.h>
#endif

static inline uint64_t now_ns_clock() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}

#ifdef __x86_64__
static inline uint64_t rdtsc() {
    return __rdtsc();
}
#else
static inline uint64_t rdtsc() { return 0; }
#endif

static int dummy() { return 42; }

int main(int argc, char **argv) {
    const long ITER = (argc > 1) ? atol(argv[1]) : 1000000L;
    const char *testfile = "/tmp/lab4_bench_testfile";

    // Ensure test file exists
    int fd = open(testfile, O_CREAT | O_RDWR, 0644);
    if (fd >= 0) {
        write(fd, "abcdefghij\n", 11);
        fsync(fd);
        close(fd);
    }

    printf("Benchmark: ITER=%ld\n", ITER);

    // 1) dummy()
    uint64_t t0 = now_ns_clock();
#ifdef __x86_64__
    uint64_t c0 = rdtsc();
#endif
    for (long i = 0; i < ITER; ++i) dummy();
#ifdef __x86_64__
    uint64_t c1 = rdtsc();
#endif
    uint64_t t1 = now_ns_clock();
    double dt_dummy_ns = (double)(t1 - t0) / ITER;
#ifdef __x86_64__
    double dc_dummy = (double)(c1 - c0) / ITER;
#else
    double dc_dummy = 0.0;
#endif

    // 2) getpid()
    t0 = now_ns_clock();
#ifdef __x86_64__
    c0 = rdtsc();
#endif
    for (long i = 0; i < ITER; ++i) { (void)getpid(); }
#ifdef __x86_64__
    c1 = rdtsc();
#endif
    t1 = now_ns_clock();
    double dt_getpid_ns = (double)(t1 - t0) / ITER;
#ifdef __x86_64__
    double dc_getpid = (double)(c1 - c0) / ITER;
#else
    double dc_getpid = 0.0;
#endif

    // 3) open+close (file on disk/cache)
    t0 = now_ns_clock();
#ifdef __x86_64__
    c0 = rdtsc();
#endif
    for (long i = 0; i < ITER; ++i) {
        int fd2 = open(testfile, O_RDONLY);
        if (fd2 >= 0) close(fd2);
    }
#ifdef __x86_64__
    c1 = rdtsc();
#endif
    t1 = now_ns_clock();
    double dt_open_ns = (double)(t1 - t0) / ITER;
#ifdef __x86_64__
    double dc_open = (double)(c1 - c0) / ITER;
#else
    double dc_open = 0.0;
#endif

    // 4) gettimeofday()
    struct timeval tv;
    t0 = now_ns_clock();
#ifdef __x86_64__
    c0 = rdtsc();
#endif
    for (long i = 0; i < ITER; ++i) {
        gettimeofday(&tv, NULL);
    }
#ifdef __x86_64__
    c1 = rdtsc();
#endif
    t1 = now_ns_clock();
    double dt_gtod_ns = (double)(t1 - t0) / ITER;
#ifdef __x86_64__
    double dc_gtod = (double)(c1 - c0) / ITER;
#else
    double dc_gtod = 0.0;
#endif

    printf("\nResults (avg per op):\n");
    printf("Operation             | ns/op (avg)   | cycles/op (avg, if available)\n");
    printf("----------------------+---------------+-----------------------------\n");
    printf("dummy()               | %12.2f | %27.2f\n", dt_dummy_ns, dc_dummy);
    printf("getpid()              | %12.2f | %27.2f\n", dt_getpid_ns, dc_getpid);
    printf("open()+close()        | %12.2f | %27.2f\n", dt_open_ns, dc_open);
    printf("gettimeofday() (vDSO) | %12.2f | %27.2f\n", dt_gtod_ns, dc_gtod);

    return 0;
}
