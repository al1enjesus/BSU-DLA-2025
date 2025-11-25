#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Current directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Lab 4 Experiments - Variant 3${NC}"
echo -e "${BLUE}Programs to analyze: gcc, make, as${NC}"
echo -e "${BLUE}Working directory: $SCRIPT_DIR${NC}"
echo -e "${BLUE}========================================${NC}"

# Create logs directory if not exists
mkdir -p logs

# Build everything
echo -e "\n${YELLOW}Building all components...${NC}"
cd src
make clean
make all
cd ..

# Task A: LD_PRELOAD Experiments
echo -e "\n${GREEN}=== TASK A: LD_PRELOAD EXPERIMENTS ===${NC}"

# Test 1: GCC compilation
echo -e "\n${YELLOW}1. Analyzing GCC compilation${NC}"
cat > src/test_prog.c << 'EOF2'
#include <stdio.h>
#include <stdlib.h>

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int main(int argc, char *argv[]) {
    printf("Factorial test program\n");
    for(int i = 0; i < argc; i++) {
        printf("Arg %d: %s\n", i, argv[i]);
    }
    printf("Factorial of 5: %d\n", factorial(5));
    return 0;
}
EOF2

echo -e "Compiling test_prog.c with gcc..."
LD_PRELOAD=./src/libsyscall_spy.so gcc -o src/test_prog src/test_prog.c 2> logs/gcc_analysis.log
echo -e "${GREEN}Full log saved to logs/gcc_analysis.log${NC}"

# Extract and display statistics for GCC
echo -e "\n${BLUE}=== GCC STATISTICS ===${NC}"
grep "SPY.*Statistics" logs/gcc_analysis.log | tail -1
echo -e "${BLUE}Top syscalls for GCC:${NC}"
grep "SPY\]" logs/gcc_analysis.log | grep -v "Statistics" | cut -d' ' -f2 | sort | uniq -c | sort -rn | head -10

# Test 2: Make
echo -e "\n${YELLOW}2. Analyzing Make${NC}"
cat > src/Makefile.test << 'EOF2'
TARGET = testprog
CC = gcc
CFLAGS = -Wall -O2

all: $(TARGET)

$(TARGET): test_prog.o util.o
$(CC) $(CFLAGS) -o $(TARGET) test_prog.o util.o

test_prog.o: test_prog.c
$(CC) $(CFLAGS) -c test_prog.c

util.o: util.c
$(CC) $(CFLAGS) -c util.c

util.c:
@echo '#include <stdio.h>' > util.c
@echo 'void util_function() { printf("Utility function\\n"); }' >> util.c

clean:
rm -f *.o $(TARGET) util.c
EOF2

cat > src/util.c << 'EOF2'
#include <stdio.h>
void util_function() { 
    printf("Utility function\n"); 
}
EOF2

echo -e "Running make..."
cd src
LD_PRELOAD=./libsyscall_spy.so make -f Makefile.test clean > /dev/null 2>&1
LD_PRELOAD=./libsyscall_spy.so make -f Makefile.test 2> ../logs/make_analysis.log
cd ..
echo -e "${GREEN}Full log saved to logs/make_analysis.log${NC}"

# Extract and display statistics for Make
echo -e "\n${BLUE}=== MAKE STATISTICS ===${NC}"
grep "SPY.*Statistics" logs/make_analysis.log | tail -1
echo -e "${BLUE}Top syscalls for Make:${NC}"
grep "SPY\]" logs/make_analysis.log | grep -v "Statistics" | cut -d' ' -f2 | sort | uniq -c | sort -rn | head -10

# Test 3: AS (assembler)
echo -e "\n${YELLOW}3. Analyzing AS (GNU Assembler)${NC}"
cat > src/test.s << 'EOF2'
.section .data
    msg: .ascii "Hello from assembly\n"
    len = . - msg

.section .text
.globl _start
_start:
    # write syscall
    mov $1, %rax        # syscall number for write
    mov $1, %rdi        # file descriptor (stdout)
    lea msg(%rip), %rsi # buffer address
    mov $len, %rdx      # buffer length
    syscall

    # exit syscall
    mov $60, %rax       # syscall number for exit
    xor %rdi, %rdi      # exit status 0
    syscall
EOF2

echo -e "Assembling test.s with as..."
LD_PRELOAD=./src/libsyscall_spy.so as src/test.s -o src/test.o 2> logs/as_analysis.log
echo -e "${GREEN}Full log saved to logs/as_analysis.log${NC}"

# Extract and display statistics for AS
echo -e "\n${BLUE}=== AS (ASSEMBLER) STATISTICS ===${NC}"
grep "SPY.*Statistics" logs/as_analysis.log | tail -1
echo -e "${BLUE}Top syscalls for AS:${NC}"
grep "SPY\]" logs/as_analysis.log | grep -v "Statistics" | cut -d' ' -f2 | sort | uniq -c | sort -rn | head -10

# Static vs Dynamic test
echo -e "\n${YELLOW}4. Testing Static vs Dynamic Linking${NC}"
echo -e "${BLUE}Dynamic binary test:${NC}"
LD_PRELOAD=./src/libsyscall_spy.so ./src/hello 2>&1 | grep "SPY.*Statistics" | tail -1

echo -e "${BLUE}Static binary test (should show no interception):${NC}"
LD_PRELOAD=./src/libsyscall_spy.so ./src/hello_static 2>&1 | head -5

# Task B: Benchmark
echo -e "\n${GREEN}=== TASK B: BENCHMARK EXPERIMENTS ===${NC}"

# Run benchmark
echo -e "\n${YELLOW}Running system call benchmark...${NC}"
./src/benchmark > logs/benchmark_output.log 2>&1
echo -e "${GREEN}Full output saved to logs/benchmark_output.log${NC}"

# Display benchmark summary
echo -e "\n${BLUE}=== BENCHMARK SUMMARY ===${NC}"
grep -A2 "=== Benchmarking" logs/benchmark_output.log
echo -e "${BLUE}Summary table:${NC}"
grep -A10 "SUMMARY TABLE" logs/benchmark_output.log | head -12

# Cache effects test
echo -e "\n${YELLOW}Testing cache effects...${NC}"
if [ "$EUID" -eq 0 ]; then
    echo -e "Dropping caches..."
    sync
    echo 3 > /proc/sys/vm/drop_caches
    echo -e "Cache effects data saved to logs/benchmark_output.log"
else
    echo -e "${RED}Note: Run with sudo to test cache effects properly${NC}"
fi

# Perf analysis
echo -e "\n${YELLOW}Running perf stat analysis...${NC}"
if command -v perf &> /dev/null; then
    perf stat -e cycles,instructions,context-switches,page-faults,cache-misses \
        ./src/benchmark > /dev/null 2> logs/perf_stats.log
    echo -e "${GREEN}Perf stats saved to logs/perf_stats.log${NC}"
    echo -e "${BLUE}Perf summary:${NC}"
    grep -E "(cycles|instructions|cache-misses|page-faults)" logs/perf_stats.log | head -5
else
    echo -e "${RED}perf not available, skipping...${NC}"
fi

# Clean up temporary files
rm -f src/test_prog.c src/test_prog src/test_prog.o src/testprog
rm -f src/test.s src/test.o src/util.c src/util.o
rm -f src/Makefile.test

echo -e "\n${BLUE}========================================${NC}"
echo -e "${BLUE}Experiments completed!${NC}"
echo -e "${BLUE}All detailed logs saved in logs/ directory${NC}"
echo -e "${BLUE}========================================${NC}"

# Display final summary
echo -e "\n${GREEN}=== FINAL SUMMARY ===${NC}"
echo -e "Log files created in logs/:"
ls -la logs/

echo -e "\n${GREEN}To view detailed results:${NC}"
echo "  cat logs/gcc_analysis.log      # Full GCC analysis"
echo "  cat logs/make_analysis.log     # Full Make analysis" 
echo "  cat logs/as_analysis.log       # Full AS analysis"
echo "  cat logs/benchmark_output.log  # Benchmark results"
echo "  cat logs/perf_stats.log        # Performance counters"
