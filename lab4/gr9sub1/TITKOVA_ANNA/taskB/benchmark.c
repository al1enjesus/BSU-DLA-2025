#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define ITERATIONS 1000000ULL
#define WARMUP_ITERATIONS 100000

// Prevent compiler optimization
static inline uint64_t blackhole(uint64_t x) {
    asm volatile("" : "+r"(x));
    return x;
}

int dummy() { return 42; }

// Function to measure overhead of measurement itself
uint64_t measure_overhead() {
    uint64_t start = __rdtsc();
    uint64_t end = __rdtsc();
    return end - start;
}

int main() {
    struct timespec ts1, ts2;
    uint64_t c1, c2;
    double ns_per_op, cycles_per_op;
    uint64_t overhead = measure_overhead();

    printf("Measurement overhead: %lu cycles\n\n", overhead);

    // ------------------------------------------------------------------
    // Warm-up phase
    // ------------------------------------------------------------------
    struct timeval tv_warmup;
    struct timespec ts_warmup;
    for (uint64_t i = 0; i < WARMUP_ITERATIONS; i++) {
        blackhole(dummy());
        blackhole(getpid());
        gettimeofday(&tv_warmup, NULL);
        clock_gettime(CLOCK_MONOTONIC, &ts_warmup);
        blackhole(tv_warmup.tv_usec);
        blackhole(ts_warmup.tv_nsec);
    }

    // Store baseline for comparison
    double dummy_ns = 0;
    //double dummy_cycles = 0;

    // ------------------------------------------------------------------
    // 1. Userspace dummy() - baseline
    // ------------------------------------------------------------------
    volatile int sink = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    c1 = __rdtsc();
    for (uint64_t i = 0; i < ITERATIONS; i++) {
        sink += dummy();
    }
    c2 = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    blackhole(sink);
    
    cycles_per_op = (double)(c2 - c1) / ITERATIONS;
    ns_per_op = (ts2.tv_sec - ts1.tv_sec) * 1e9 + (ts2.tv_nsec - ts1.tv_nsec);
    ns_per_op /= ITERATIONS;
    printf("dummy() (userspace): %6.2f cycles   %8.2f ns\n", cycles_per_op, ns_per_op);
    dummy_ns = ns_per_op;
    //dummy_cycles = cycles_per_op;

    // ------------------------------------------------------------------
    // 2. getpid() - fast syscall
    // ------------------------------------------------------------------
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    c1 = __rdtsc();
    for (uint64_t i = 0; i < ITERATIONS; i++) {
        blackhole(getpid());
    }
    c2 = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    
    cycles_per_op = (double)(c2 - c1) / ITERATIONS;
    ns_per_op = (ts2.tv_sec - ts1.tv_sec) * 1e9 + (ts2.tv_nsec - ts1.tv_nsec);
    ns_per_op /= ITERATIONS;
    printf("getpid() (fast):     %6.2f cycles   %8.2f ns\n", cycles_per_op, ns_per_op);
    double getpid_slowdown = ns_per_op / dummy_ns;

    // ------------------------------------------------------------------
    // 3. open() + close() - slow syscall (disk I/O)
    // ------------------------------------------------------------------
    const char *path = "./testfile_benchmark";
    // Create test file (1MB) - ignore return value intentionally
    int ret = system("dd if=/dev/zero of=./testfile_benchmark bs=1M count=1 status=none 2>/dev/null");
    (void)ret; // explicitly ignore return value
    
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    c1 = __rdtsc();
    for (uint64_t i = 0; i < ITERATIONS; i++) {
        int fd = open(path, O_RDONLY);
        if (fd >= 0) close(fd);
    }
    c2 = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    
    cycles_per_op = (double)(c2 - c1) / ITERATIONS;
    ns_per_op = (ts2.tv_sec - ts1.tv_sec) * 1e9 + (ts2.tv_nsec - ts1.tv_nsec);
    ns_per_op /= ITERATIONS;
    printf("open+close (cached): %6.2f cycles   %8.2f ns\n", cycles_per_op, ns_per_op);
    double open_slowdown = ns_per_op / dummy_ns;

    // ------------------------------------------------------------------
    // 4. gettimeofday() - vDSO optimized
    // ------------------------------------------------------------------
    struct timeval tv;
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    c1 = __rdtsc();
    for (uint64_t i = 0; i < ITERATIONS; i++) {
        gettimeofday(&tv, NULL);
        blackhole(tv.tv_usec);
    }
    c2 = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &ts2);
    
    cycles_per_op = (double)(c2 - c1) / ITERATIONS;
    ns_per_op = (ts2.tv_sec - ts1.tv_sec) * 1e9 + (ts2.tv_nsec - ts1.tv_nsec);
    ns_per_op /= ITERATIONS;
    printf("gettimeofday (vDSO): %6.2f cycles   %8.2f ns\n", cycles_per_op, ns_per_op);
    double vdso_slowdown = ns_per_op / dummy_ns;

    // ------------------------------------------------------------------
    // 5. clock_gettime() - also vDSO optimized (FIXED measurement)
    // ------------------------------------------------------------------
    struct timespec ts_inner;  // Separate variable for inner measurement
    clock_gettime(CLOCK_MONOTONIC, &ts1);
    c1 = __rdtsc();
    for (uint64_t i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &ts_inner);  // Use separate variable
        blackhole(ts_inner.tv_nsec);
    }
    c2 = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC, &ts2);

    cycles_per_op = (double)(c2 - c1) / ITERATIONS;
    ns_per_op = (ts2.tv_sec - ts1.tv_sec) * 1e9 + (ts2.tv_nsec - ts1.tv_nsec);
    ns_per_op /= ITERATIONS;
    printf("clock_gettime(vDSO): %6.2f cycles   %8.2f ns\n", cycles_per_op, ns_per_op);
    double clock_slowdown = ns_per_op / dummy_ns;

    // Cleanup
    unlink(path);
    
    printf("\nRelative slowdown compared to dummy():\n");
    printf("getpid():       %.0fx slower\n", getpid_slowdown);
    printf("open+close:     %.0fx slower\n", open_slowdown);
    printf("gettimeofday:   %.0fx slower\n", vdso_slowdown);
    printf("clock_gettime:  %.0fx slower\n", clock_slowdown);

    return 0;
}
