#include <iostream>
#include <x86intrin.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <cstdio>
#include <cstdint>

static inline uint64_t rdtsc() { return __rdtsc(); }
#define ITER 25

int dummy() { return 42; }
inline void empty_func() {}
volatile int global_counter = 0;
void increment() { global_counter++; }

void measure(const char *name, bool is_syscall, void (*func)())
{
    uint64_t sum = 0;
    for (int i = 0; i < ITER; ++i)
    {
        uint64_t start = rdtsc();
        func();
        uint64_t end = rdtsc();
        sum += (end - start);
    }
    double avg = (double)sum / ITER;
    printf("| %-18s | %-10s | %10.2f |\n",
           name, is_syscall ? "syscall" : "userspace", avg);
}

// Обёртки для системных вызовов
void f_getpid() { getpid(); }
void f_gettimeofday()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
}
void f_time() { time(nullptr); }
void f_clock_gettime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
}
void f_open_close()
{
    int fd = open("/tmp/testfile", O_RDONLY | O_CREAT, 0644);
    close(fd);
}
void f_stat()
{
    struct stat sb;
    stat("/etc/passwd", &sb);
}
void f_write() { write(1, "", 0); }
void f_read()
{
    int fd = open("/dev/null", O_RDONLY);
    read(fd, nullptr, 0);
    close(fd);
}
void f_sleep() { usleep(0); }
void f_getcwd()
{
    char buf[256];
    getcwd(buf, sizeof(buf));
}

// Обёртки для userspace
void f_dummy() { dummy(); }
void f_empty() { empty_func(); }
void f_increment() { increment(); }
void f_arithmetic()
{
    volatile int x = 3 * 5 + 7;
    (void)x;
}
void f_loop()
{
    volatile int x = 0;
    for (int i = 0; i < 10; i++)
        x += i;
}

int main()
{
    printf("| %-18s | %-10s | %10s |\n", "Function", "Type", "Avg cycles");
    printf("|--------------------|------------|------------|\n");

    // userspace
    measure("dummy()", false, f_dummy);
    measure("empty_func()", false, f_empty);
    measure("increment()", false, f_increment);
    measure("arithmetic()", false, f_arithmetic);
    measure("small_loop()", false, f_loop);

    // syscalls
    measure("getpid()", true, f_getpid);
    measure("gettimeofday()", true, f_gettimeofday);
    measure("time()", true, f_time);
    measure("clock_gettime()", true, f_clock_gettime);
    measure("open+close()", true, f_open_close);
    measure("stat()", true, f_stat);
    measure("write(0B)", true, f_write);
    measure("read(0B)", true, f_read);
    measure("usleep(0)", true, f_sleep);
    measure("getcwd()", true, f_getcwd);

    printf("|--------------------|------------|------------|\n");
}
