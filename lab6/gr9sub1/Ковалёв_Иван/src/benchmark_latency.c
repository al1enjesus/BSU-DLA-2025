#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define BUF_SIZE 4096
#define ITERATIONS 1000

void measure_getattr(const char *path) {
    struct stat st;
    struct timespec start, end;
    long long total = 0;
    
    for (int i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        stat(path, &st);
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        long long elapsed = (end.tv_sec - start.tv_sec) * 1000000000 + 
                           (end.tv_nsec - start.tv_nsec);
        total += elapsed;
    }
    
    printf("getattr: %.3f ms\n", (total / (double)ITERATIONS) / 1000000);
}

void measure_open_close(const char *path) {
    struct timespec start, end;
    long long total = 0;
    
    for (int i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        int fd = open(path, O_RDONLY);
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        if (fd >= 0) close(fd);
        
        long long elapsed = (end.tv_sec - start.tv_sec) * 1000000000 + 
                           (end.tv_nsec - start.tv_nsec);
        total += elapsed;
    }
    
    printf("open: %.3f ms\n", (total / (double)ITERATIONS) / 1000000);
}

void measure_read(const char *path) {
    char buf[BUF_SIZE];
    struct timespec start, end;
    long long total = 0;
    
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }
    
    for (int i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        ssize_t res = read(fd, buf, BUF_SIZE);
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        if (res < 0) {
            perror("read");
            break;
        }
        
        lseek(fd, 0, SEEK_SET); // Reset to beginning for next read
        
        long long elapsed = (end.tv_sec - start.tv_sec) * 1000000000 + 
                           (end.tv_nsec - start.tv_nsec);
        total += elapsed;
    }
    
    close(fd);
    printf("read: %.3f ms\n", (total / (double)ITERATIONS) / 1000000);
}

void measure_write(const char *path) {
    char buf[BUF_SIZE];
    memset(buf, 'A', BUF_SIZE);
    struct timespec start, end;
    long long total = 0;
    
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return;
    }
    
    for (int i = 0; i < ITERATIONS; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        ssize_t res = write(fd, buf, BUF_SIZE);
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        if (res < 0) {
            perror("write");
            break;
        }
        
        ftruncate(fd, 0); // Reset file size
        lseek(fd, 0, SEEK_SET);
        
        long long elapsed = (end.tv_sec - start.tv_sec) * 1000000000 + 
                           (end.tv_nsec - start.tv_nsec);
        total += elapsed;
    }
    
    close(fd);
    unlink(path);
    printf("write: %.3f ms\n", (total / (double)ITERATIONS) / 1000000);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mount_point> <test_file>\n", argv[0]);
        return 1;
    }
    
    char test_path[1024];
    snprintf(test_path, sizeof(test_path), "%s/%s", argv[1], argv[2]);
    
    printf("=== Latency Benchmark for %s ===\n", argv[1]);
    measure_getattr(test_path);
    measure_open_close(test_path);
    measure_read(test_path);
    measure_write(test_path);
    
    return 0;
}
