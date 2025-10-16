CC=gcc
CFLAGS=-fPIC -Wall -Werror
LDFLAGS_SPY=-shared -ldl

# Directories
BIN_DIR=bin
LIB_DIR=lib
LOG_DIR=logs

# Main targets
MYTRACER=$(BIN_DIR)/mytracer
BENCHMARK=$(BIN_DIR)/benchmark
BENCHMARK_OPEN=$(BIN_DIR)/benchmark_open
HELLO_STATIC=$(BIN_DIR)/hello_static
SYSCALL_SPY_LIB=$(LIB_DIR)/libsyscall_spy.so

# All executables and libraries to be built by 'all'
BUILD_TARGETS=$(MYTRACER) $(BENCHMARK) $(HELLO_STATIC) $(SYSCALL_SPY_LIB)

.PHONY: all clean run_ls run_cat run_grep run_benchmark run_static_test perf_benchmark

all: $(BUILD_TARGETS)

# Ensure directories exist before building any target that needs them
$(BUILD_TARGETS) $(BENCHMARK_OPEN): | $(BIN_DIR) $(LIB_DIR)

# Rule to create directories
$(BIN_DIR) $(LIB_DIR) $(LOG_DIR):
	mkdir -p $@

# Build rules
$(SYSCALL_SPY_LIB): src/syscall_spy.c
	$(CC) $(CFLAGS) $(LDFLAGS_SPY) -o $@ $<

$(HELLO_STATIC): src/hello.c
	$(CC) -static -o $@ $<

$(BENCHMARK): src/benchmark.c
	$(CC) -o $@ $<

$(BENCHMARK_OPEN): src/benchmark_open.c
	$(CC) -o $@ $<

$(MYTRACER): src/mytracer.c src/syscall_names.h
	$(CC) -o $@ $(filter %.c,$^)

# Runner rules
run_benchmark: $(BENCHMARK) | $(LOG_DIR)
	./$(BENCHMARK) > $(LOG_DIR)/benchmark.log

perf_benchmark: $(BENCHMARK_OPEN) | $(LOG_DIR)
	perf stat -e cycles,instructions,context-switches,page-faults ./$(BENCHMARK_OPEN) > $(LOG_DIR)/perf_benchmark.log 2>&1

run_ls: $(SYSCALL_SPY_LIB) | $(LOG_DIR)
	LD_PRELOAD=./$(SYSCALL_SPY_LIB) ls -la > $(LOG_DIR)/ls.log 2>&1

run_cat: $(SYSCALL_SPY_LIB) | $(LOG_DIR)
	LD_PRELOAD=./$(SYSCALL_SPY_LIB) cat /etc/passwd > $(LOG_DIR)/cat.log 2>&1

run_grep: $(SYSCALL_SPY_LIB) | $(LOG_DIR)
	LD_PRELOAD=./$(SYSCALL_SPY_LIB) grep root /etc/passwd > $(LOG_DIR)/grep.log 2>&1

run_static_test: $(SYSCALL_SPY_LIB) $(HELLO_STATIC) | $(LOG_DIR)
	LD_PRELOAD=./$(SYSCALL_SPY_LIB) ./$(HELLO_STATIC) > $(LOG_DIR)/static.log 2>&1

clean:
	rm -rf $(LIB_DIR) $(BIN_DIR) $(LOG_DIR)
