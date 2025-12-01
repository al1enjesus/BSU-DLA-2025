#include <iostream>
#include <iomanip>
#include <cstdint>
#include <ctime>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <x86intrin.h>
#include <vector>
#include <algorithm>
#include <cmath>

constexpr int BENCHMARK_ITERATIONS = 1000000; // Основные измерения
auto TEST_FILE = "/tmp/syscall_benchmark_test.txt";

static void create_test_file() {
    const int fd = open(TEST_FILE, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Cannot create test file");
        exit(1);
    }
    write(fd, "test\n", 5);
    close(fd);
}

struct BenchmarkResult {
    std::string name{};
    double avg_ns{};
    double min_ns{};
    double max_ns{};
    double median_ns{};
    double stddev_ns{};
    uint64_t avg_cycles{};
};


inline uint64_t get_time_ns() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
}

inline uint64_t get_cpu_cycles() {
    const auto res = __rdtsc();
    return res;
}

void calculate_statistics(std::vector<double>& samples, BenchmarkResult& result) {
    if (samples.empty()) return;
    
    std::sort(samples.begin(), samples.end());
    
    result.min_ns = samples.front();
    result.max_ns = samples.back();
    result.median_ns = samples[samples.size() / 2];
    
    double sum = 0;
    for (const double& val : samples) sum += val;
    result.avg_ns = sum / static_cast<double>(samples.size());
    
    double variance = 0;
    for (const double& val : samples) {
        const double diff = val - result.avg_ns;
        variance += diff * diff;
    }

    result.stddev_ns = std::sqrt(variance / static_cast<double>(samples.size() > 1 ? samples.size() - 1 : 1));
}

__attribute__((noinline))
int dummy_function() {
    return 33;
}

void print_results(const std::vector<BenchmarkResult>& results) {
    std::cout << "\n═══════════════════════════════════════════════════════════════════\n";
    std::cout << "                    BENCHMARK RESULTS\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n\n";
    
    // Заголовок таблицы
    std::cout << std::left << std::setw(30) << "Operation"
              << std::right << std::setw(12) << "Avg (ns)"
              << std::setw(12) << "Median (ns)"
              << std::setw(12) << "Min (ns)"
              << std::setw(12) << "Cycles"
              << std::setw(10) << "Slowdown"
              << "\n";
    std::cout << std::string(88, '-') << "\n";
    
    const double baseline = results[0].avg_ns;
    
    for (const auto& r : results) {
        const double slowdown = r.avg_ns / baseline;
        
        std::cout << std::left << std::setw(30) << r.name
                  << std::right << std::setw(12) << std::fixed << std::setprecision(2) << r.avg_ns
                  << std::setw(12) << r.median_ns
                  << std::setw(12) << r.min_ns
                  << std::setw(12) << r.avg_cycles
                  << std::setw(9) << std::setprecision(1) << slowdown << "x"
                  << "\n";
    }
    
    std::cout << "\n═══════════════════════════════════════════════════════════════════\n\n";
    
    // Детальная статистика
    std::cout << "Detailed Statistics:\n";
    std::cout << std::string(88, '-') << "\n";
    for (const auto& r : results) {
        std::cout << r.name << ":\n";
        std::cout << "  Average:   " << std::fixed << std::setprecision(2) << r.avg_ns << " ns\n";
        std::cout << "  Median:    " << r.median_ns << " ns\n";
        std::cout << "  Min:       " << r.min_ns << " ns\n";
        std::cout << "  Max:       " << r.max_ns << " ns\n";
        std::cout << "  Std Dev:   " << r.stddev_ns << " ns\n";
        std::cout << "  CPU Cycles: " << r.avg_cycles << "\n\n";
    }
}

BenchmarkResult benchmark(const std::string& name, void(*f)(), const uint32_t chunk_size) {
    std::vector<double> samples;
    samples.reserve(BENCHMARK_ITERATIONS / chunk_size);

    for (int batch = 0; batch < BENCHMARK_ITERATIONS / chunk_size; batch++) {
        const uint64_t start_time = get_time_ns();

        for (int i = 0; i < chunk_size; i++) {
            f();
        }

        const uint64_t end_time = get_time_ns();
        double time_per_call = static_cast<double>(end_time - start_time) / chunk_size;
        samples.push_back(time_per_call);
    }

    BenchmarkResult result;
    result.name = name;
    calculate_statistics(samples, result);

    // Измерение циклов CPU
    const uint64_t cycles_start = get_cpu_cycles();
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        f();
    }
    const uint64_t cycles_end = get_cpu_cycles();
    result.avg_cycles = (cycles_end - cycles_start) / BENCHMARK_ITERATIONS;

    return result;
}

int main() {
    create_test_file();

    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "         SYSCALL PERFORMANCE BENCHMARK (Lab 4, Task B)\n";
    std::cout << "═══════════════════════════════════════════════════════════════════\n";
    std::cout << "\nConfiguration:\n";
    std::cout << "  Benchmark iterations: " << BENCHMARK_ITERATIONS << "\n";
    std::cout << "  Test file:            " << TEST_FILE << "\n";
    std::cout << "\nStarting benchmarks...\n";
    
    std::vector<BenchmarkResult> results;

    std::cout << "\n[1/4] Benchmarking userspace function (dummy)...\n";
    results.push_back(benchmark("dummy() [userspace]", [](){volatile int sink = dummy_function();}, 1000));

    std::cout << "[2/4] Benchmarking getpid() syscall...\n";
    results.push_back(benchmark("getpid()", [](){getpid();}, 1000));

    std::cout << "[3/4] Benchmarking open()+close() syscall...\n";
    results.push_back(benchmark("open()+close()",
        [](){
            const int fd = open(TEST_FILE, O_RDONLY);
            if (fd != -1) close(fd);
        }, 100));

    std::cout << "[4/4] Benchmarking gettimeofday() [vDSO]...\n";
    results.push_back(benchmark("gettimeofday() [vDSO]",
        []() {
            struct timeval tv{};
            gettimeofday(&tv, nullptr);
        }, 1000));

    print_results(results);

    std::cout << "Benchmark completed successfully!\n";
    std::cout << "\nRecommended follow-up commands:\n";
    std::cout << "  perf stat -e cycles,instructions,context-switches,page-faults ./benchmark\n";
    std::cout << "  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches' && ./benchmark\n";
    std::cout << "\n";
    return 0;
}
