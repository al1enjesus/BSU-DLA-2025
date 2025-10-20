#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <x86intrin.h> 
#include <stdlib.h>

static inline uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000000ULL + t.tv_nsec;
}

volatile int sink_int = 0; 

int dummy(void) {
    return 42;
}

int measure_dummy_cycles(int iterations) {
    int ret = 0;
    int (*fp)() = dummy;
    uint64_t s = __rdtsc();
    for (int i = 0; i < iterations; i++) {
        ret += fp();
    }
    uint64_t e = __rdtsc();
    sink_int = ret;
    return (int)(e - s);
}

uint64_t measure_dummy_ns(int iterations) {
    int (*fp)() = dummy;
    uint64_t s = now_ns();
    for (int i = 0; i < iterations; i++) {
        sink_int += fp();
    }
    uint64_t e = now_ns();
    return e - s;
}

uint64_t measure_getpid_cycles(int iterations) {
    uint64_t s = __rdtsc();
    for (int i = 0; i < iterations; i++) {
        pid_t p = getpid();
        (void)p;
    }
    uint64_t e = __rdtsc();
    return e - s;
}

uint64_t measure_gettimeofday_ns(int iterations) {
    uint64_t s = now_ns();
    for (int i = 0; i < iterations; i++) {
        struct timespec t;
        clock_gettime(CLOCK_REALTIME, &t);
        (void)t.tv_sec;
    }
    uint64_t e = now_ns();
    return e - s;
}

uint64_t measure_open_close_ns(int iterations, const char *path) {
    uint64_t s = now_ns();
    for (int i = 0; i < iterations; i++) {
        int fd = open(path, O_RDONLY);
        if (fd >= 0) close(fd);
    }
    uint64_t e = now_ns();
    return e - s;
}

int main(int argc, char **argv) {
    int iters = 1000000;
    if (argc > 1) iters = atoi(argv[1]);

    printf("Iters=%d\n", iters);
    for (int i = 0; i < 1000; i++) dummy();

    uint64_t cycles_dummy = measure_dummy_cycles(iters);
    uint64_t ns_dummy = measure_dummy_ns(iters);

    uint64_t ns_getpid = 0;
    {
        uint64_t s = now_ns();
        for (int i = 0; i < iters; i++) {
            getpid();
        }
        uint64_t e = now_ns();
        ns_getpid = e - s;
    }
    



    uint64_t ns_gettime = measure_gettimeofday_ns(iters);

    const char *path = "/tmp/benchmark_testfile";
    int tfd = open(path, O_CREAT | O_WRONLY, 0644);
    if (tfd >= 0) { write(tfd, "test\n", 5); close(tfd); }
    uint64_t ns_open = measure_open_close_ns(100000, path);
    uint64_t ns_open_per = ns_open / 100000;

    printf("\nRESULTS (averages):\n");
    double avg_dummy_ns = (double)ns_dummy / iters;
    double avg_dummy_cycles = (double)cycles_dummy / iters;
    printf("dummy()         : %f ns, %f cycles\n", avg_dummy_ns, avg_dummy_cycles);
#
    printf("getpid()        : %f ns\n", (double)ns_getpid / iters);
    printf("gettimeofday()  : %f ns\n", (double)ns_gettime / iters);
    printf("open()+close()  : %f ns (measured, %d iterations)\n", (double)ns_open_per, 100000);

    printf("\nRaw cycles (dummy total): %llu\n", (unsigned long long)cycles_dummy);
    printf("Sink: %d\n", sink_int);

    return 0;
}
