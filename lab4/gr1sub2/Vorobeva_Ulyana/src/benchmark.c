#define _GNU_SOURCE
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static inline long diff_ns(struct timespec a, struct timespec b) {
    return (a.tv_sec - b.tv_sec) * 1000000000L + (a.tv_nsec - b.tv_nsec);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <iterations>\n", argv[0]);
        return 1;
    }
    long N = atol(argv[1]);
    if (N <= 0) N = 100000;

    // Benchmark open+close
    struct timespec tstart, tend;
    char tmpname[] = "/tmp/lab4_bench_XXXXXX";
    int fdtmp = mkstemp(tmpname);
    if (fdtmp >= 0) close(fdtmp);

    clock_gettime(CLOCK_MONOTONIC, &tstart);
    for (long i = 0; i < N; ++i) {
        int fd = open(tmpname, O_RDONLY);
        if (fd >= 0) close(fd);
    }
    clock_gettime(CLOCK_MONOTONIC, &tend);
    long total_ns = diff_ns(tend, tstart);
    printf("open+close: iterations=%ld total_ns=%ld avg_ns=%.2f\n", N, total_ns, (double)total_ns / N);

    // Benchmark write to /dev/null
    int fdnull = open("/dev/null", O_WRONLY);
    if (fdnull < 0) { perror("open /dev/null"); return 2; }
    const char buf[64] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n";
    clock_gettime(CLOCK_MONOTONIC, &tstart);
    for (long i = 0; i < N; ++i) {
        ssize_t r = write(fdnull, buf, sizeof(buf));
        (void)r;
    }
    clock_gettime(CLOCK_MONOTONIC, &tend);
    total_ns = diff_ns(tend, tstart);
    printf("write(/dev/null): iterations=%ld total_ns=%ld avg_ns=%.2f\n", N, total_ns, (double)total_ns / N);

    unlink(tmpname);
    close(fdnull);
    return 0;
}
