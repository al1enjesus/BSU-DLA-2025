
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

#define ITERATIONS 1000000

int dummy() { return 42; }

void test_gettimeofday() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
}

void test_open_close() {
    int fd = open("/tmp/testfile", O_RDONLY);
    if (fd >= 0) close(fd);
}

int main() {
    system("echo 'test' > /tmp/testfile");
    
    struct timespec start, end;
    double time_ns;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ITERATIONS; i++) dummy();
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    double t_dummy = time_ns / ITERATIONS;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ITERATIONS; i++) getpid();
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    double t_getpid = time_ns / ITERATIONS;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ITERATIONS; i++) test_gettimeofday();
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    double t_gettimeofday = time_ns / ITERATIONS;
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ITERATIONS; i++) test_open_close();
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
    double t_openclose = time_ns / ITERATIONS;
    
    printf("| Операция           | Время (ns) | Циклов CPU | Во сколько раз медленнее userspace |\n");
    printf("|--------------------|------------|------------|------------------------------------|\n");
    printf("| dummy()            | %-10.0f | %-10.0f | %-33s |\n", t_dummy, t_dummy * 2.6, "1x (базовая линия)");
    printf("| getpid()           | %-10.0f | %-10.0f | %-33.0fx |\n", t_getpid, t_getpid * 2.6, t_getpid / t_dummy);
    printf("| gettimeofday vDSO  | %-10.0f | %-10.0f | %-33.0fx |\n", t_gettimeofday, t_gettimeofday * 2.6, t_gettimeofday / t_dummy);
    printf("| open+close         | %-10.0f | %-10.0f | %-33.0fx |\n", t_openclose, t_openclose * 2.6, t_openclose / t_dummy);
    
    system("rm -f /tmp/testfile");
    return 0;
}