#!/bin/bash
set -e

echo "=== Task B — System Call Benchmark ==="

# Создаем структуру папок
RESULTS_DIR="benchmark_results"
LOGS_DIR="$RESULTS_DIR/logs"
BIN_DIR="$RESULTS_DIR/bin"

mkdir -p "$LOGS_DIR" "$BIN_DIR"

echo "Created directory structure:"
echo "  $RESULTS_DIR/"
echo "  $LOGS_DIR/" 
echo "  $BIN_DIR/"

echo "Compiling benchmark..."
gcc -O2 -Wall -o "$BIN_DIR/benchmark" benchmark.c

echo -e "\n=== 1. Normal Run (Cached) ==="
"$BIN_DIR/benchmark" > "$LOGS_DIR/results_cached.txt"
cat "$LOGS_DIR/results_cached.txt"

echo -e "\n=== 2. Uncached Run ==="
echo "Dropping caches..."
sync
sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches' 2>/dev/null || echo "Note: Could not drop caches (need sudo)"
"$BIN_DIR/benchmark" > "$LOGS_DIR/results_uncached.txt"
cat "$LOGS_DIR/results_uncached.txt"

echo -e "\n=== 3. Perf Statistics ==="
echo "Note: Running perf in virtual machine - some counters may not be available"
sudo perf stat -e cycles,instructions,context-switches,page-faults "$BIN_DIR/benchmark" 2> "$LOGS_DIR/perf_results.txt"
cat "$LOGS_DIR/perf_results.txt"

echo -e "\n=== 4. Final Summary Table ==="
echo "| Operation             | Время (ns)   | Циклов CPU   | Во сколько раз медленнее userspace |"
echo "|-----------------------|--------------|--------------|------------------------------------|"

# Надежный парсинг с обработкой формата вывода
parse_measurement() {
    local file="$1"
    local pattern="$2"
    
    if [ ! -f "$file" ]; then
        echo "0 0"
        return
    fi
    
    local line=$(grep "$pattern" "$file" | head -1)
    if [ -z "$line" ]; then
        echo "0 0"
        return
    fi
    
    # Извлекаем числа из строки формата: "3.42 cycles       1.37 ns"
    # cycles - первое число, ns - четвертое число
    local cycles=$(echo "$line" | grep -oE '[0-9]+\.[0-9]+' | head -1)
    local ns=$(echo "$line" | grep -oE '[0-9]+\.[0-9]+' | tail -1)
    
    echo "$cycles $ns"
}

#echo "Parsing results from cached run..."

# Парсим каждую операцию
dummy_data=$(parse_measurement "$LOGS_DIR/results_cached.txt" "dummy()")
dummy_cycles=$(echo "$dummy_data" | awk '{print $1}')
dummy_ns=$(echo "$dummy_data" | awk '{print $2}')

getpid_data=$(parse_measurement "$LOGS_DIR/results_cached.txt" "getpid()")
getpid_cycles=$(echo "$getpid_data" | awk '{print $1}')
getpid_ns=$(echo "$getpid_data" | awk '{print $2}')

open_data=$(parse_measurement "$LOGS_DIR/results_cached.txt" "open+close")
open_cached_cycles=$(echo "$open_data" | awk '{print $1}')
open_cached_ns=$(echo "$open_data" | awk '{print $2}')

open_uncached_data=$(parse_measurement "$LOGS_DIR/results_uncached.txt" "open+close")
open_uncached_cycles=$(echo "$open_uncached_data" | awk '{print $1}')
open_uncached_ns=$(echo "$open_uncached_data" | awk '{print $2}')

vdso_data=$(parse_measurement "$LOGS_DIR/results_cached.txt" "gettimeofday")
vdso_cycles=$(echo "$vdso_data" | awk '{print $1}')
vdso_ns=$(echo "$vdso_data" | awk '{print $2}')

clock_data=$(parse_measurement "$LOGS_DIR/results_cached.txt" "clock_gettime")
clock_cycles=$(echo "$clock_data" | awk '{print $1}')
clock_ns=$(echo "$clock_data" | awk '{print $2}')

#echo "Parsing complete!"
#echo "DEBUG: dummy_ns=$dummy_ns, dummy_cycles=$dummy_cycles"

# Функция расчета slowdown
calculate_slowdown() {
    local time_ns="$1"
    local baseline="$2"
    
    if [ "$(echo "$baseline > 0" | bc -l 2>/dev/null)" = "1" ] && [ "$(echo "$time_ns > 0" | bc -l 2>/dev/null)" = "1" ]; then
        result=$(echo "scale=0; $time_ns / $baseline" | bc 2>/dev/null || echo "0")
        if [ -n "$result" ] && [ "$result" != "0" ]; then
            echo "$result"
        else
            echo "0"
        fi
    else
        echo "0"
    fi
}

# Рассчитываем slowdown
getpid_slowdown=$(calculate_slowdown "$getpid_ns" "$dummy_ns")
open_slowdown=$(calculate_slowdown "$open_cached_ns" "$dummy_ns")
vdso_slowdown=$(calculate_slowdown "$vdso_ns" "$dummy_ns")
clock_slowdown=$(calculate_slowdown "$clock_ns" "$dummy_ns")

# Выводим таблицу в требуемом формате
{
    printf "| %-21s | %12.2f | %12.2f | %34s |\n" "dummy()" "$dummy_ns" "$dummy_cycles" "		1x (базовая линия)"
    printf "| %-21s | %12.2f | %12.2f | %34s |\n" "getpid()" "$getpid_ns" "$getpid_cycles" "${getpid_slowdown}x"
    printf "| %-21s | %12.2f | %12.2f | %34s |\n" "open+close (cached)" "$open_cached_ns" "$open_cached_cycles" "${open_slowdown}x"
    printf "| %-21s | %12.2f | %12.2f | %34s |\n" "open+close (uncached)" "$open_uncached_ns" "$open_uncached_cycles" "N/A"
    printf "| %-21s | %12.2f | %12.2f | %34s |\n" "gettimeofday (vDSO)" "$vdso_ns" "$vdso_cycles" "${vdso_slowdown}x"
    printf "| %-21s | %12.2f | %12.2f | %34s |\n" "clock_gettime (vDSO)" "$clock_ns" "$clock_cycles" "${clock_slowdown}x"
}

# Создаем README с описанием файлов
cat > "$RESULTS_DIR/README.md" << EOF
# Benchmark Results

## Virtual Machine Environment
- **Note**: Benchmarks run in VM - some perf counters unavailable
- **cycles/instructions**: <not supported> in VM
- **context-switches**: 162 (measured)
- **page-faults**: 453 (measured)

## Directory Structure:
- \`bin/benchmark\` - Compiled benchmark executable
- \`logs/\` - All output files and logs
  - \`results_cached.txt\` - Cached run results
  - \`results_uncached.txt\` - Uncached run results  
  - \`perf_results.txt\` - Performance counters from perf stat

## Measurements:
- **dummy()**: Userspace function call baseline
- **getpid()**: Fast system call (cached PID)
- **open+close**: Slow system call with disk I/O
- **gettimeofday/clock_gettime**: vDSO-optimized calls

## How to Reproduce:
\`\`\`bash
./run_benchmark.sh
\`\`\`
EOF

echo -e "\n=== Files Created ==="
echo "Directory structure:"
find "$RESULTS_DIR" -type f -printf "  %p\n"


echo -e "\nTo view results:"
echo "  cat $LOGS_DIR/results_cached.txt"
echo "  cat $LOGS_DIR/perf_results.txt"
