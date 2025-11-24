#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#define ITERATIONS 100000
#define FILENAME "/tmp/benchmark_open_test_file.txt"

void run_open_benchmark() {
    struct timespec start, end;
    long total_ns = 0;

    // Создаем файл
    int fd = open(FILENAME, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("Error creating test file");
        exit(1);
    }
    close(fd);

    // Измерение
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < ITERATIONS; i++) {
        int temp_fd = open(FILENAME, O_RDONLY);
        if (temp_fd >= 0) {
            close(temp_fd);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    total_ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
    
    printf("Среднее время open()/close() (100k итераций): %.2f ns\n", 
           (double)total_ns / ITERATIONS);
    
    // Удаляем файл
    unlink(FILENAME);
}

int main() {
    run_open_benchmark();
    return 0;
}