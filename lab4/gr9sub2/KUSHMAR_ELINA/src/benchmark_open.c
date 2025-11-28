#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

long long get_nanotime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

int main() {
    const int iterations = 10000;
    long long start, end;
    int i;
    
    start = get_nanotime();
    for (i = 0; i < iterations; i++) {
        int fd = open("/tmp/cache_test", O_CREAT | O_RDWR, 0644);
        if (fd != -1) close(fd);
    }
    end = get_nanotime();
    
    printf("open()+close(): %.2f ns/call\n", (double)(end - start) / iterations);
    remove("/tmp/cache_test");
    return 0;
}
