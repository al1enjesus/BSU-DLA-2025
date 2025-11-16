#include <iostream>
#include <fstream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <x86intrin.h>

int dummy() { return 42; }

double get_cpu_ghz() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("cpu MHz") != std::string::npos) {
            auto pos = line.find(":");
            if (pos != std::string::npos) {
                double mhz = std::stod(line.substr(pos + 1));
                return mhz / 1000.0;
            }
        }
    }
    return 3.0;
}

int main() {
    const int iterations = 1000000;
    uint64_t start, end;
    uint64_t dummy_cycles = 0, getpid_cycles = 0, open_cycles = 0, gettimeofday_cycles = 0;

    for (int i = 0; i < iterations; ++i) {
        start = __rdtsc();
        dummy();
        end = __rdtsc();
        dummy_cycles += (end - start);
    }

    for (int i = 0; i < iterations; ++i) {
        start = __rdtsc();
        getpid();
        end = __rdtsc();
        getpid_cycles += (end - start);
    }

    for (int i = 0; i < iterations; ++i) {
        start = __rdtsc();
        int fd = open("/tmp/bench_test.txt", O_RDWR | O_CREAT, 0666);
        close(fd);
        end = __rdtsc();
        open_cycles += (end - start);
    }

    struct timeval tv;
    for (int i = 0; i < iterations; ++i) {
        start = __rdtsc();
        gettimeofday(&tv, NULL);
        end = __rdtsc();
        gettimeofday_cycles += (end - start);
    }

    double ns_per_cycle = 1.0 / get_cpu_ghz();

    double dummy_ns = double(dummy_cycles) / iterations * ns_per_cycle;
    double getpid_ns = double(getpid_cycles) / iterations * ns_per_cycle;
    double open_ns = double(open_cycles) / iterations * ns_per_cycle;
    double gettimeofday_ns = double(gettimeofday_cycles) / iterations * ns_per_cycle;

    auto print_result = [&](const char* name, double ns, double cycles) {
        double relative = ns / dummy_ns;
        printf("%-20s | %7.2f ns | %7.1f cycles | %5.1fx\n", name, ns, cycles, relative);
    };

    printf("| Операция           | Время (ns) | Циклов CPU | Во сколько раз медленнее userspace |\n");
    printf("|--------------------|------------|------------|------------------------------------|\n");
    print_result("dummy()", dummy_ns, double(dummy_cycles) / iterations);
    print_result("getpid()", getpid_ns, double(getpid_cycles) / iterations);
    print_result("open+close", open_ns, double(open_cycles) / iterations);
    print_result("gettimeofday vDSO", gettimeofday_ns, double(gettimeofday_cycles) / iterations);

    return 0;
}
