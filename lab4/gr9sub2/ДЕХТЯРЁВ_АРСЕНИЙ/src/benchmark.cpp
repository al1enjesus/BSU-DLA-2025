#include <x86intrin.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdint>
#include <cstdio>

static inline uint64_t rdtsc() {
    return __rdtsc();
}

int dummy() {
    return 42;
}

uint64_t bench_dummy(int iters) {
    uint64_t total = 0;
    for (int i = 0; i < iters; i++) {
        uint64_t t1 = rdtsc();
        dummy();
        uint64_t t2 = rdtsc();
        total += (t2 - t1);
    }
    return total / iters;
}

uint64_t bench_getpid(int iters) {
    uint64_t total = 0;
    for (int i = 0; i < iters; i++) {
        uint64_t t1 = rdtsc();
        getpid();
        uint64_t t2 = rdtsc();
        total += (t2 - t1);
    }
    return total / iters;
}

uint64_t bench_openclose(int iters) {
    uint64_t total = 0;
    for (int i = 0; i < iters; i++) {
        uint64_t t1 = rdtsc();
        int fd = open("/tmp/testfile", O_RDONLY);
        if (fd >= 0) close(fd);
        uint64_t t2 = rdtsc();
        total += (t2 - t1);
    }
    return total / iters;
}

uint64_t bench_gettimeofday(int iters) {
    struct timeval tv;
    uint64_t total = 0;
    for (int i = 0; i < iters; i++) {
        uint64_t t1 = rdtsc();
        gettimeofday(&tv, NULL);
        uint64_t t2 = rdtsc();
        total += (t2 - t1);
    }
    return total / iters;
}

int main() {
    const int iters = 1'000'000;
    printf("Running benchmark with %d iterations...\n\n", iters);

    uint64_t c_dummy = bench_dummy(iters);
    uint64_t c_getpid = bench_getpid(iters);
    uint64_t c_open = bench_openclose(iters);
    uint64_t c_vdso = bench_gettimeofday(iters);

    printf("Average cycles per operation:\n");
    printf("---------------------------------\n");
    printf("dummy()            : %lu cycles\n", c_dummy);
    printf("getpid()           : %lu cycles\n", c_getpid);
    printf("open()+close()     : %lu cycles\n", c_open);
    printf("gettimeofday vDSO  : %lu cycles\n", c_vdso);

    return 0;
}
