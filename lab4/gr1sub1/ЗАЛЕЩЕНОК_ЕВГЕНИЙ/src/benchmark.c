#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define ITERATIONS 1000000
#define CPU_FREQ_GHZ 2.7

// 1. Быстрая функция в userspace
__attribute__((noinline))
int dummy_func() {
    return 42;
}

// 2. Быстрый системный вызов
__attribute__((noinline))
pid_t getpid_call() {
    return getpid();
}

// 3. Медленный системный вызов
__attribute__((noinline))
int open_close_call() {
    int fd = open("/tmp/testfile", O_WRONLY | O_CREAT, 0644);
    if (fd == -1) return -1;
    return close(fd);
}

// 4. vDSO-вызов
__attribute__((noinline))
int gettimeofday_call(struct timeval *tv) {
    return gettimeofday(tv, NULL);
}

int main() {
    uint64_t start, end;
    uint64_t total_cycles = 0;
    struct timeval tv;

    // --- Измерение dummy_func ---
    for (int i = 0; i < ITERATIONS; ++i) {
        start = __rdtsc();
        dummy_func();
        end = __rdtsc();
        total_cycles += (end - start);
    }
    double avg_dummy = (double)total_cycles / ITERATIONS;
    printf("1. Userspace dummy():\t\tAvg Cycles: %.2f, Avg Time: %.2f ns\n",
           avg_dummy, avg_dummy / CPU_FREQ_GHZ);

    // --- Измерение getpid() ---
    total_cycles = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        start = __rdtsc();
        getpid_call();
        end = __rdtsc();
        total_cycles += (end - start);
    }
    double avg_getpid = (double)total_cycles / ITERATIONS;
    printf("2. Syscall getpid():\t\tAvg Cycles: %.2f, Avg Time: %.2f ns, Slower: %.2fx\n",
           avg_getpid, avg_getpid / CPU_FREQ_GHZ, avg_getpid / avg_dummy);

    // --- Измерение open()+close() ---
    total_cycles = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        start = __rdtsc();
        open_close_call();
        end = __rdtsc();
        total_cycles += (end - start);
    }
    double avg_open_close = (double)total_cycles / ITERATIONS;
    printf("3. Syscall open()+close():\tAvg Cycles: %.2f, Avg Time: %.2f ns, Slower: %.2fx\n",
           avg_open_close, avg_open_close / CPU_FREQ_GHZ, avg_open_close / avg_dummy);

    // --- Измерение gettimeofday() ---
    total_cycles = 0;
    for (int i = 0; i < ITERATIONS; ++i) {
        start = __rdtsc();
        gettimeofday_call(&tv);
        end = __rdtsc();
        total_cycles += (end - start);
    }
    double avg_gettimeofday = (double)total_cycles / ITERATIONS;
    printf("4. vDSO gettimeofday():\t\tAvg Cycles: %.2f, Avg Time: %.2f ns, Slower: %.2fx\n",
           avg_gettimeofday, avg_gettimeofday / CPU_FREQ_GHZ, avg_gettimeofday / avg_dummy);

    return 0;
}
