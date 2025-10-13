#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <x86intrin.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#define ITERATIONS 1000000
#define CPU_FREQ_GHZ 3.3

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

uint64_t measure_cycles(void (*func)()) {
    uint64_t start, end;
    start = __rdtsc();
    for (int i = 0; i < ITERATIONS; i++) {
        func();
    }
    end = __rdtsc();
    return (end - start) / ITERATIONS;
}

__attribute__((noinline))
int dummy() {
    return 42;
}

void getpid_call() {
    getpid();
}

void open_close_call() {
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "Error: HOME environment variable not set\n");
        return;
    }

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/testfile", home);

    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        fprintf(stderr, "Error: open failed for %s: %s\n", path, strerror(errno));
        return;
    }

    if (close(fd) == -1) {
        fprintf(stderr, "Error: close failed: %s\n", strerror(errno));
    }
}

void gettimeofday_call() {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) {
        fprintf(stderr, "Error: gettimeofday failed: %s\n", strerror(errno));
    }
}

int main() {
    uint64_t dummy_cycles = measure_cycles((void (*)())dummy);
    uint64_t getpid_cycles = measure_cycles(getpid_call);
    uint64_t open_close_cycles = measure_cycles(open_close_call);
    uint64_t gettimeofday_cycles = measure_cycles(gettimeofday_call);

    double dummy_ns = (double)dummy_cycles / CPU_FREQ_GHZ;
    double getpid_ns = (double)getpid_cycles / CPU_FREQ_GHZ;
    double open_close_ns = (double)open_close_cycles / CPU_FREQ_GHZ;
    double gettimeofday_ns = (double)gettimeofday_cycles / CPU_FREQ_GHZ;

    printf("Benchmark results (1,000,000 iterations):\n");
    printf("--------------------------------------------------------------------------------------\n");
    printf("| Operation                 | Avg. Cycles | Avg. Time (ns) | Slower than userspace   |\n");
    printf("--------------------------------------------------------------------------------------\n");
    printf("| dummy() userspace         | %-11lu | %-14.2f | 1.00x                   |\n", dummy_cycles, dummy_ns);
    printf("| getpid()                  | %-11lu | %-14.2f | %.2fx                  |\n", getpid_cycles, getpid_ns, (double)getpid_cycles / dummy_cycles);
    printf("| open(\"file_name\")+close() | %-11lu | %-14.2f | %.2fx                |\n", open_close_cycles, open_close_ns, (double)open_close_cycles / dummy_cycles);
    printf("| gettimeofday() vDSO       | %-11lu | %-14.2f | %.2fx                  |\n", gettimeofday_cycles, gettimeofday_ns, (double)gettimeofday_cycles / dummy_cycles);
    printf("--------------------------------------------------------------------------------------\n");
    printf("Note: CPU frequency is set to %.2f GHz. Please adjust if needed.\n", CPU_FREQ_GHZ);

    return 0;
}
