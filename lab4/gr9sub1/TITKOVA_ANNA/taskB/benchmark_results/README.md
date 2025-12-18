# Benchmark Results

## Virtual Machine Environment
- **Note**: Benchmarks run in VM - some perf counters unavailable
- **cycles/instructions**: <not supported> in VM
- **context-switches**: 162 (measured)
- **page-faults**: 453 (measured)

## Directory Structure:
- `bin/benchmark` - Compiled benchmark executable
- `logs/` - All output files and logs
  - `results_cached.txt` - Cached run results
  - `results_uncached.txt` - Uncached run results  
  - `perf_results.txt` - Performance counters from perf stat

## Measurements:
- **dummy()**: Userspace function call baseline
- **getpid()**: Fast system call (cached PID)
- **open+close**: Slow system call with disk I/O
- **gettimeofday/clock_gettime**: vDSO-optimized calls

## How to Reproduce:
```bash
./run_benchmark.sh
```
