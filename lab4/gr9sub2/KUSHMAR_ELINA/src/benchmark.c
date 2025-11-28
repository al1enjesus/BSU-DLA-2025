#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <stddef.h>

// Быстрая userspace функция
int dummy() { return 42; }

// Функция для измерения времени в наносекундах
long long get_nanotime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main() {
    const int iterations = 1000000;
    long long start, end;
    int i;
    
    printf("=== BENCHMARK СИСТЕМНЫХ ВЫЗОВОВ ===\n");
    printf("Итераций: %d\n\n", iterations);

    // 1. Измерение dummy() - userspace функция
    start = get_nanotime();
    for (i = 0; i < iterations; i++) {
        dummy();
    }
    end = get_nanotime();
    printf("dummy() userspace: %.2f ns/call\n", (double)(end - start) / iterations);

    // 2. Измерение getpid() - быстрый syscall
    start = get_nanotime();
    for (i = 0; i < iterations; i++) {
        getpid();
    }
    end = get_nanotime();
    printf("getpid() syscall:  %.2f ns/call\n", (double)(end - start) / iterations);

    // 3. Измерение gettimeofday() - vDSO
    struct timeval tv;
    start = get_nanotime();
    for (i = 0; i < iterations; i++) {
        gettimeofday(&tv, NULL);
    }
    end = get_nanotime();
    printf("gettimeofday vDSO: %.2f ns/call\n", (double)(end - start) / iterations);

    // 4. Измерение open()+close() - медленный syscall
    start = get_nanotime();
    for (i = 0; i < iterations / 10; i++) { // Меньше итераций - очень медленно
        int fd = open("/tmp/bench_test", O_CREAT | O_RDWR, 0644);
        if (fd != -1) close(fd);
    }
    end = get_nanotime();
    printf("open()+close():   %.2f ns/call\n", (double)(end - start) / (iterations / 10));

    // Убираем временный файл
    remove("/tmp/bench_test");

    return 0;
}
