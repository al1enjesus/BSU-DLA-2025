#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <x86intrin.h>
#include <stdlib.h>

#define N_ITER 1000000UL 
#define WARMUP 10000UL

__attribute__((noinline))
volatile int dummy(void) {
    volatile int x = 42;
    return x;
}

__attribute__((noinline))
volatile pid_t call_getpid(void) {
    return getpid();
}

__attribute__((noinline))
int call_gettimeofday(void) {
    struct timeval tv;
    return gettimeofday(&tv, NULL);
}

__attribute__((noinline))
int call_open_close(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return 0;
    }
    return -1;
}

static inline uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

uint64_t measure_cycles(void (*op)(void), unsigned long n) {
    // warmup
    for (unsigned long i = 0; i < WARMUP; ++i) op();

    unsigned long long sum = 0;
    unsigned int aux;
    for (unsigned long i = 0; i < n; ++i) {
        unsigned long long t1 = __rdtsc();
        op();
        unsigned long long t2 = __rdtscp(&aux);
        sum += (t2 - t1);
    }
    return (uint64_t)(sum / n);
}

uint64_t measure_cycles_intret(int (*op)(void), unsigned long n) {
    for (unsigned long i = 0; i < WARMUP; ++i) op();

    unsigned long long sum = 0;
    unsigned int aux;
    for (unsigned long i = 0; i < n; ++i) {
        unsigned long long t1 = __rdtsc();
        (void)op();
        unsigned long long t2 = __rdtscp(&aux);
        sum += (t2 - t1);
    }
    return (uint64_t)(sum / n);
}

uint64_t measure_ns_intret(int (*op)(void), unsigned long n) {
    for (unsigned long i = 0; i < WARMUP; ++i) op();

    uint64_t t0 = now_ns();
    for (unsigned long i = 0; i < n; ++i) {
        (void)op();
    }
    uint64_t t1 = now_ns();
    return (t1 - t0) / n; // среднее ns
}

int main(int argc, char **argv) {
    const char *tmpfile = "/tmp/lab4_bench_file";
    int fd = open(tmpfile, O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        write(fd, "bench\n", 6);
        close(fd);
    }

    unsigned long n = N_ITER;
    printf("Benchmark: %'lu iterations\n", n);

    uint64_t c_dummy = measure_cycles((void(*)(void))dummy, n);
    uint64_t ns_dummy = measure_ns_intret((int(*)(void))dummy, n);
    printf("dummy(): cycles(avg) = %" PRIu64 ", time(avg) = %" PRIu64 " ns\n",
           c_dummy, ns_dummy);

    uint64_t c_getpid = measure_cycles_intret((int(*)(void)) (void*)call_getpid, n);
    uint64_t ns_getpid = measure_ns_intret((int(*)(void)) (void*)call_getpid, n);
    printf("getpid(): cycles(avg) = %" PRIu64 ", time(avg) = %" PRIu64 " ns\n",
           c_getpid, ns_getpid);

    uint64_t c_openclose;
    uint64_t ns_openclose;
    if (n > 2000000UL) n = 2000000UL;
    {
        unsigned long long sum = 0;
        unsigned int aux;
        for (unsigned long i = 0; i < n; ++i) {
            unsigned long long t1 = __rdtsc();
            call_open_close(tmpfile);
            unsigned long long t2 = __rdtscp(&aux);
            sum += (t2 - t1);
        }
        c_openclose = sum / n;
    }
    {
        uint64_t t0 = now_ns();
        for (unsigned long i = 0; i < n; ++i) {
            call_open_close(tmpfile);
        }
        uint64_t t1 = now_ns();
        ns_openclose = (t1 - t0) / n;
    }
    printf("open+close(\"%s\"): cycles(avg) = %" PRIu64 ", time(avg) = %" PRIu64 " ns\n",
           tmpfile, c_openclose, ns_openclose);

    uint64_t c_gtd = measure_cycles_intret(call_gettimeofday, n);
    uint64_t ns_gtd = measure_ns_intret(call_gettimeofday, n);
    printf("gettimeofday(): cycles(avg) = %" PRIu64 ", time(avg) = %" PRIu64 " ns\n",
           c_gtd, ns_gtd);

    printf("\n=== Summary (avg per op) ===\n");
    printf("%-20s %-15s %-15s\n", "Operation", "Cycles(avg)", "Time(avg) ns");
    printf("%-20s %-15" PRIu64 " %-15" PRIu64 "\n", "dummy()", c_dummy, ns_dummy);
    printf("%-20s %-15" PRIu64 " %-15" PRIu64 "\n", "getpid()", c_getpid, ns_getpid);
    printf("%-20s %-15" PRIu64 " %-15" PRIu64 "\n", "open+close", c_openclose, ns_openclose);
    printf("%-20s %-15" PRIu64 " %-15" PRIu64 "\n", "gettimeofday()", c_gtd, ns_gtd);

    return 0;
}
