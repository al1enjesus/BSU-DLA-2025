#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

#define ITERATIONS 1000000

long long diff_ns(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000000LL + (end.tv_nsec - start.tv_nsec);
}

void regular_function() {
    volatile int x = 0;
    x++;
}

int main() {
    struct timespec start, end;
    long long total_syscall = 0;
    long long total_func = 0;

    printf("Benchmarking system calls vs regular functions\n");

    for (int i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        syscall(SYS_getpid);
        clock_gettime(CLOCK_MONOTONIC, &end);
        total_syscall += diff_ns(start, end);
    }

    for (int i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        regular_function();
        clock_gettime(CLOCK_MONOTONIC, &end);
        total_func += diff_ns(start, end);
    }

    printf("Average syscall time: %.2f ns\n", (double) total_syscall / ITERATIONS);
    printf("Average regular function time: %.2f ns\n", (double) total_func / ITERATIONS);

    return 0;
}
