/**
 * benchmark.c - System call timing benchmark
 * Lab 4, Variant 19
 * 
 * Measures: dummy function, getpid(), open+close, gettimeofday()
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <x86intrin.h>
#include <stdint.h>
#include <errno.h>

#define ITERATIONS 1000000
#define WARMUP_ITERATIONS 1000

// Test file for open/close benchmark
#define TEST_FILE "/tmp/benchmark_test_file"

// Prevent compiler optimization
volatile int dummy_result;

// Simple userspace function (no syscall)
__attribute__((noinline))
int dummy_function() {
    return 42;
}

// Get CPU frequency for cycle-to-nanosecond conversion
double get_cpu_freq_ghz() {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) return 2.5; // Default fallback
    
    char line[256];
    double freq = 2.5;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu MHz", 7) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                freq = atof(colon + 1) / 1000.0; // Convert MHz to GHz
                break;
            }
        }
    }
    fclose(fp);
    return freq;
}

// High-resolution timing using RDTSC
typedef struct {
    uint64_t start;
    uint64_t end;
    uint64_t cycles;
    double nanoseconds;
} timing_t;

void start_timing(timing_t *t) {
    __asm__ volatile ("mfence" ::: "memory"); // Memory fence for accuracy
    t->start = __rdtsc();
}

void end_timing(timing_t *t, double cpu_freq_ghz) {
    t->end = __rdtsc();
    __asm__ volatile ("mfence" ::: "memory");
    t->cycles = t->end - t->start;
    t->nanoseconds = (double)t->cycles / cpu_freq_ghz;
}

// Safe write function that handles errors
void safe_write(int fd, const void *buf, size_t count) {
    ssize_t result = write(fd, buf, count);
    if (result < 0) {
        perror("write failed");
    }
}

// Safe read function that handles errors
void safe_read(int fd, void *buf, size_t count) {
    ssize_t result = read(fd, buf, count);
    if (result < 0) {
        perror("read failed");
    }
}

// Benchmark a userspace function
void benchmark_userspace() {
    printf("\n=== Benchmarking userspace function (dummy) ===\n");
    
    timing_t t;
    double cpu_freq = get_cpu_freq_ghz();
    
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        dummy_result = dummy_function();
    }
    
    // Actual measurement
    start_timing(&t);
    for (int i = 0; i < ITERATIONS; i++) {
        dummy_result = dummy_function();
    }
    end_timing(&t, cpu_freq);
    
    double avg_cycles = (double)t.cycles / ITERATIONS;
    double avg_ns = t.nanoseconds / ITERATIONS;
    
    printf("Total time: %.2f ms\n", t.nanoseconds / 1000000.0);
    printf("Average per call: %.2f ns (%.1f cycles)\n", avg_ns, avg_cycles);
    printf("Calls per second: %.0f million\n", 1000.0 / avg_ns);
}

// Benchmark getpid() - fast syscall
void benchmark_getpid() {
    printf("\n=== Benchmarking getpid() syscall ===\n");
    
    timing_t t;
    double cpu_freq = get_cpu_freq_ghz();
    pid_t pid;
    
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        pid = getpid();
    }
    
    // Actual measurement
    start_timing(&t);
    for (int i = 0; i < ITERATIONS; i++) {
        pid = getpid();
    }
    end_timing(&t, cpu_freq);
    
    double avg_cycles = (double)t.cycles / ITERATIONS;
    double avg_ns = t.nanoseconds / ITERATIONS;
    
    printf("Total time: %.2f ms\n", t.nanoseconds / 1000000.0);
    printf("Average per call: %.2f ns (%.1f cycles)\n", avg_ns, avg_cycles);
    printf("Calls per second: %.0f million\n", 1000.0 / avg_ns);
    printf("PID value: %d\n", pid);
}

// Benchmark open+close - slow syscall
void benchmark_open_close() {
    printf("\n=== Benchmarking open()+close() syscalls ===\n");
    
    // Create test file first
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to create test file");
        return;
    }
    safe_write(fd, "test", 4);
    close(fd);
    
    timing_t t;
    double cpu_freq = get_cpu_freq_ghz();
    
    // Warmup
    for (int i = 0; i < 100; i++) { // Less warmup for slow syscalls
        fd = open(TEST_FILE, O_RDONLY);
        if (fd >= 0) close(fd);
    }
    
    // Actual measurement
    int iterations = 10000; // Less iterations for slow syscalls
    start_timing(&t);
    for (int i = 0; i < iterations; i++) {
        fd = open(TEST_FILE, O_RDONLY);
        if (fd >= 0) {
            close(fd);
        }
    }
    end_timing(&t, cpu_freq);
    
    double avg_cycles = (double)t.cycles / iterations;
    double avg_ns = t.nanoseconds / iterations;
    
    printf("Total time: %.2f ms for %d iterations\n", 
           t.nanoseconds / 1000000.0, iterations);
    printf("Average per open+close pair: %.2f ns (%.1f cycles)\n", 
           avg_ns, avg_cycles);
    printf("Calls per second: %.0f thousand\n", 1000000.0 / avg_ns);
    
    // Cleanup
    unlink(TEST_FILE);
}

// Benchmark gettimeofday() - vDSO optimized
void benchmark_gettimeofday() {
    printf("\n=== Benchmarking gettimeofday() vDSO call ===\n");
    
    timing_t t;
    double cpu_freq = get_cpu_freq_ghz();
    struct timeval tv;
    
    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        gettimeofday(&tv, NULL);
    }
    
    // Actual measurement
    start_timing(&t);
    for (int i = 0; i < ITERATIONS; i++) {
        gettimeofday(&tv, NULL);
    }
    end_timing(&t, cpu_freq);
    
    double avg_cycles = (double)t.cycles / ITERATIONS;
    double avg_ns = t.nanoseconds / ITERATIONS;
    
    printf("Total time: %.2f ms\n", t.nanoseconds / 1000000.0);
    printf("Average per call: %.2f ns (%.1f cycles)\n", avg_ns, avg_cycles);
    printf("Calls per second: %.0f million\n", 1000.0 / avg_ns);
    printf("Current time: %ld.%06ld\n", tv.tv_sec, tv.tv_usec);
}

// Test cache effects on open()
void benchmark_cache_effects() {
    printf("\n=== Testing cache effects on open() ===\n");
    
    // Create test file
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to create test file");
        return;
    }
    // Write some data to make it non-trivial
    char buffer[4096];
    memset(buffer, 'A', sizeof(buffer));
    for (int i = 0; i < 10; i++) {
        safe_write(fd, buffer, sizeof(buffer));
    }
    close(fd);
    
    timing_t t;
    double cpu_freq = get_cpu_freq_ghz();
    int iterations = 1000;
    
    printf("\nNote: Run 'sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches' ");
    printf("to clear cache before cold cache test\n");
    
    // Cold cache test (first run after cache clear)
    printf("\n1. Cold cache test (first access):\n");
    start_timing(&t);
    for (int i = 0; i < iterations; i++) {
        fd = open(TEST_FILE, O_RDONLY);
        if (fd >= 0) {
            char buf[1];
            safe_read(fd, buf, 1); // Force actual file access
            close(fd);
        }
    }
    end_timing(&t, cpu_freq);
    
    double cold_avg_ns = t.nanoseconds / iterations;
    double cold_avg_cycles = (double)t.cycles / iterations;
    printf("   Average: %.2f ns (%.1f cycles)\n", cold_avg_ns, cold_avg_cycles);
    
    // Warm cache test (repeated access)
    printf("\n2. Warm cache test (repeated access):\n");
    start_timing(&t);
    for (int i = 0; i < iterations; i++) {
        fd = open(TEST_FILE, O_RDONLY);
        if (fd >= 0) {
            char buf[1];
            safe_read(fd, buf, 1);
            close(fd);
        }
    }
    end_timing(&t, cpu_freq);
    
    double warm_avg_ns = t.nanoseconds / iterations;
    double warm_avg_cycles = (double)t.cycles / iterations;
    printf("   Average: %.2f ns (%.1f cycles)\n", warm_avg_ns, warm_avg_cycles);
    
    printf("\n3. Cache speedup: %.2fx faster when cached\n", 
           cold_avg_ns / warm_avg_ns);
    
    // Cleanup
    unlink(TEST_FILE);
}

// Safe system call that handles errors
void safe_system(const char *command) {
    int result = system(command);
    if (result == -1) {
        perror("system call failed");
    }
}

// Print summary table
void print_summary_table() {
    printf("\n" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" 
           "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" 
           "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" 
           "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=\n");
    printf("SUMMARY TABLE (re-run all tests for accurate comparison)\n");
    printf("-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" 
           "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" 
           "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" 
           "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-\n");
    
    double cpu_freq = get_cpu_freq_ghz();
    timing_t t;
    
    // Quick re-run for summary
    double results[4][2]; // [test][0=ns, 1=cycles]
    
    // 1. Userspace
    start_timing(&t);
    for (int i = 0; i < ITERATIONS; i++) {
        dummy_result = dummy_function();
    }
    end_timing(&t, cpu_freq);
    results[0][0] = t.nanoseconds / ITERATIONS;
    results[0][1] = (double)t.cycles / ITERATIONS;
    
    // 2. getpid
    start_timing(&t);
    for (int i = 0; i < ITERATIONS; i++) {
        getpid();
    }
    end_timing(&t, cpu_freq);
    results[1][0] = t.nanoseconds / ITERATIONS;
    results[1][1] = (double)t.cycles / ITERATIONS;
    
    // 3. open+close
    int iterations = 10000;
    start_timing(&t);
    for (int i = 0; i < iterations; i++) {
        int fd = open(TEST_FILE, O_RDONLY);
        if (fd >= 0) close(fd);
    }
    end_timing(&t, cpu_freq);
    results[2][0] = t.nanoseconds / iterations;
    results[2][1] = (double)t.cycles / iterations;
    
    // 4. gettimeofday
    struct timeval tv;
    start_timing(&t);
    for (int i = 0; i < ITERATIONS; i++) {
        gettimeofday(&tv, NULL);
    }
    end_timing(&t, cpu_freq);
    results[3][0] = t.nanoseconds / ITERATIONS;
    results[3][1] = (double)t.cycles / ITERATIONS;
    
    printf("| Operation               | Time (ns) | CPU Cycles | Slowdown vs userspace |\n");
    printf("|-------------------------|-----------|------------|-----------------------|\n");
    printf("| dummy() userspace       | %9.2f | %10.1f | 1.0x (baseline)       |\n",
           results[0][0], results[0][1]);
    printf("| getpid()                | %9.2f | %10.1f | %.1fx                  |\n",
           results[1][0], results[1][1], results[1][0] / results[0][0]);
    printf("| open() + close()        | %9.2f | %10.1f | %.1fx                |\n",
           results[2][0], results[2][1], results[2][0] / results[0][0]);
    printf("| gettimeofday() vDSO     | %9.2f | %10.1f | %.1fx                  |\n",
           results[3][0], results[3][1], results[3][0] / results[0][0]);
    printf("-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" 
           "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" 
           "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" 
           "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" "-\n");
    
    printf("\nCPU Frequency: %.2f GHz\n", cpu_freq);
    printf("Test iterations: %d (except open+close: %d)\n", ITERATIONS, iterations);
}

int main() {
    printf("========================================\n");
    printf("System Call Benchmark - Lab 4\n");
    printf("Variant 19 (Programs: gcc, make, as)\n");
    printf("========================================\n");
    
    printf("\nSystem info:\n");
    printf("CPU: ");
    fflush(stdout);
    safe_system("grep 'model name' /proc/cpuinfo | head -1 | cut -d':' -f2");
    printf("Kernel: ");
    fflush(stdout);
    safe_system("uname -r");
    
    // Individual benchmarks with detailed output
    benchmark_userspace();
    benchmark_getpid();
    benchmark_open_close();
    benchmark_gettimeofday();
    benchmark_cache_effects();
    
    // Summary table
    print_summary_table();
    
    printf("\n========================================\n");
    printf("Benchmark completed!\n");
    printf("========================================\n");
    
    return 0;
}
