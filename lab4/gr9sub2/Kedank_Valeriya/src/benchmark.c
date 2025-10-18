#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

int dummy() {
    return 42;
}

// Функция для получения времени в наносекундах
long long get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main() {
    printf("=== Benchmark системных вызовов ===\n");
    
    int iterations = 1000000;
    printf("Итераций: %d\n\n", iterations);
    
    // 1. Измеряем dummy()
    long long start = get_time_ns();
    for (int i = 0; i < iterations; i++) {
        dummy();
    }
    long long end = get_time_ns();
    long long dummy_time = (end - start) / iterations;
    
    // 2. Измеряем getpid()
    start = get_time_ns();
    for (int i = 0; i < iterations; i++) {
        getpid();
    }
    end = get_time_ns();
    long long getpid_time = (end - start) / iterations;
    
    // 3. Измеряем gettimeofday() (vDSO)
    struct timeval tv;
    start = get_time_ns();
    for (int i = 0; i < iterations; i++) {
        gettimeofday(&tv, NULL);
    }
    end = get_time_ns();
    long long gettime_time = (end - start) / iterations;
    
    printf("Результаты (среднее время):\n");
    printf("dummy():         %lld ns (1x)\n", dummy_time);
    printf("getpid():        %lld ns (%.1fx)\n", getpid_time, (double)getpid_time/dummy_time);
    printf("gettimeofday():  %lld ns (%.1fx)\n", gettime_time, (double)gettime_time/dummy_time);
    
    return 0;
}
