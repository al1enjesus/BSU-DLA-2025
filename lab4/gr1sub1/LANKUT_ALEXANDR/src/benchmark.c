#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <x86intrin.h>
#include <sys/types.h>
#include <sys/stat.h>

#define ITERATIONS 100000
#define TEST_FILE "logs/benchmark_testfile.txt"
#define TEST_DATA "Test data\n"
#define ERROR_CODE_FILE_OPEN -1
#define NSEC_PER_SEC 1000000000ULL  /* Количество наносекунд в секунде */

static inline unsigned long long fast_add(unsigned long long a) {
    return a + 1;
}

static unsigned long long get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
}

static unsigned long long measure_cycles(void (*func)(void)) {
    unsigned long long start, end;
    start = __rdtsc();
    func();
    end = __rdtsc();
    return end - start;
}

static void test_userspace(void) {
    volatile unsigned long long x = 0;
    x = fast_add(x);
}

static void test_getpid(void) {
    getpid();
}

static void test_open(void) {
    int fd = open(TEST_FILE, O_RDONLY);
    if (fd >= 0) close(fd);
}

static void test_gettimeofday(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
}

int main() {
    FILE *f = fopen(TEST_FILE, "w");
    if (!f) {
        perror("Failed to open test file");
        return ERROR_CODE_FILE_OPEN;
    }

    if (fprintf(f, TEST_DATA) < 0) {
        perror("Failed to write test data");
        fclose(f);
        return ERROR_CODE_FILE_OPEN;
    }

    if (fclose(f) != 0) {
        perror("Failed to close test file");
        return ERROR_CODE_FILE_OPEN;
    }

    printf("Running each test %d times...\n\n", ITERATIONS);

    unsigned long long cycles[4][ITERATIONS];
    unsigned long long times[4][ITERATIONS];

    // Run measurements
    for (int i = 0; i < ITERATIONS; i++) {
        unsigned long long start_ns;
        
        // 1. Fast userspace function
        start_ns = get_time_ns();
        cycles[0][i] = measure_cycles(test_userspace);
        times[0][i] = get_time_ns() - start_ns;

        // 2. Fast syscall (getpid)
        start_ns = get_time_ns();
        cycles[1][i] = measure_cycles(test_getpid);
        times[1][i] = get_time_ns() - start_ns;

        // 3. Slow syscall (open)
        start_ns = get_time_ns();
        cycles[2][i] = measure_cycles(test_open);
        times[2][i] = get_time_ns() - start_ns;

        // 4. vDSO call (clock_gettime)
        start_ns = get_time_ns();
        cycles[3][i] = measure_cycles(test_gettimeofday);
        times[3][i] = get_time_ns() - start_ns;
    }

    // Calculate and print averages
    const char *names[] = {
        "Userspace function",
        "Fast syscall (getpid)",
        "Slow syscall (open)",
        "vDSO call (clock_gettime)"
    };

    printf("Results:\n");
    printf("%-25s %15s %15s\n", "Operation", "Cycles", "Time (ns)");
    printf("---------------------------------------------------------\n");

    for (int t = 0; t < 4; t++) {
        unsigned long long total_cycles = 0, total_time = 0;
        for (int i = 0; i < ITERATIONS; i++) {
            total_cycles += cycles[t][i];
            total_time += times[t][i];
        }
        
        double avg_cycles = (double)total_cycles / ITERATIONS;
        double avg_time = (double)total_time / ITERATIONS;

        printf("%-25s %15.2f %15.2f\n", names[t], avg_cycles, avg_time);
    }

    unlink(TEST_FILE);
    return 0;
}