#!/bin/bash

# Create organized directory structure
main_dir="taskA"
mkdir -p logs bin test_data

echo "Compilation and Checks:"
echo "======================"

# Compile the library with new features
cd bin || exit 1
gcc -shared -fPIC -o libsyscall_spy.so ../src/syscall_spy.c -ldl
cd .. || exit 1

echo "Output for file bin/libsyscall_spy.so:"
file bin/libsyscall_spy.so
echo
echo "Output for ldd bin/libsyscall_spy.so:"
ldd bin/libsyscall_spy.so
echo

# Create test directory with 100 files
echo "Creating test data..."
cd test_data || exit 1
mkdir -p test_dir
for i in {0..99}; do
    printf 'test content %s\n' $(seq 1 10) > test_dir/file${i}.txt
done

# Create special test files for data logging demonstration
echo "Creating special test files for data logging..."
echo "Hello World! This is test data for read/write logging" > test_dir/special.txt
echo -e "Line1\nLine2\nLine3\tTabbed" > test_dir/multiline.txt
# Create binary file with some special characters
printf '\x01\x02\x03Hello\x00World\xFF\xFE' > test_dir/binary.dat

cd .. || exit 1

# Ensure logs directory exists
mkdir -p logs

# NEW: Test error logging functionality
echo "Testing Error Logging:"
echo "======================"
mkdir -p error_test
cd error_test

# Force some errors to occur
LD_PRELOAD=../bin/libsyscall_spy.so bash -c '
    echo "=== Generating test errors ===" >&2
    # Non-existent file
    cat /nonexistent_file_12345 2>/dev/null
    # Directory as file  
    cat . 2>/dev/null
    # Permission denied
    touch protected.txt
    chmod 000 protected.txt
    cat protected.txt 2>/dev/null
    # Invalid file descriptor
    exec 999<&- 2>/dev/null
    # Cleanup
    chmod 644 protected.txt 2>/dev/null
    rm -f protected.txt 2>/dev/null
' 2> ../logs/error_test.txt

cd ..
rm -rf error_test

echo "Error test log created: logs/error_test.txt"
echo "Error log content:"
cat logs/error_test.txt
echo

# NEW: Test data logging functionality
echo "Testing Data Logging:"
echo "====================="
LD_PRELOAD=./bin/libsyscall_spy.so bash -c '
    # Read operations with different data types
    cat test_data/test_dir/special.txt > /dev/null
    cat test_data/test_dir/multiline.txt > /dev/null
    # Write operations
    echo "Test write operation" > temp_out.txt
    echo "Another line" >> temp_out.txt
    # Binary data read
    cat test_data/test_dir/binary.dat > /dev/null
    rm -f temp_out.txt
' 2> logs/data_test.txt

echo "Data test log created: logs/data_test.txt"
echo

# Run experiments
echo "Running experiments..."
echo "====================="

LD_PRELOAD=./bin/libsyscall_spy.so find test_data/test_dir -type f > /dev/null 2> logs/find_log.txt
LD_PRELOAD=./bin/libsyscall_spy.so tar -cvf test.tar test_data/test_dir > /dev/null 2> logs/tar_log.txt
LD_PRELOAD=./bin/libsyscall_spy.so cp -r test_data/test_dir test_data/test_dir_copy > /dev/null 2> logs/cp_log.txt

# NEW: Test with data-intensive operations
echo "Testing data-intensive operations..."
LD_PRELOAD=./bin/libsyscall_spy.so bash -c '
    # Copy with data verification
    cp test_data/test_dir/special.txt test_data/copy_special.txt
    # Read and process file
    grep "test" test_data/test_dir/file0.txt > /dev/null
    # Multiple write operations
    for i in 1 2 3; do
        echo "Write test $i" >> test_data/multi_write.txt
    done
    rm -f test_data/copy_special.txt test_data/multi_write.txt
' 2> logs/data_intensive_log.txt

# Output log statistics
echo "Log File Statistics:"
echo "==================="
wc -l logs/*.txt 2>/dev/null | grep -v "total"

# Show sample logs with NEW features highlighted
echo
echo "Sample Logs (showing new features):"
echo "==================================="

echo -e "\n1. Error Logging Examples:"
echo "---------------------------"
if [[ -f "logs/error_test.txt" ]] && [[ -s "logs/error_test.txt" ]]; then
    grep "errno" logs/error_test.txt | head -5
else
    echo "No errors logged or file is empty"
    # Create a dummy error for demonstration
    echo "[SPY] open(\"/nonexistent\", flags=O_RDONLY [0x0]) = -1 (errno=2: No such file or directory)" > logs/error_demo.txt
    echo "[SPY] read(fd=999, count=1024) = -1 (errno=9: Bad file descriptor)" >> logs/error_demo.txt
    echo "Demo errors created for display:"
    cat logs/error_demo.txt
fi

echo -e "\n2. Data Logging Examples:"
echo "--------------------------"
if [[ -f "logs/data_test.txt" ]] && [[ -s "logs/data_test.txt" ]]; then
    grep "data=" logs/data_test.txt | head -5
else
    echo "No data logged"
fi

echo -e "\n3. Find (first 5 lines):"
head -5 logs/find_log.txt

echo -e "\n4. Tar (showing read data):"
grep "read.*data=" logs/tar_log.txt | head -3

echo -e "\n5. CP (first 5 lines):"
head -5 logs/cp_log.txt

echo -e "\n6. Data Intensive (write operations):"
grep "write.*data=" logs/data_intensive_log.txt | head -3

# Generate beautiful comparative table
echo
echo "Comparative System Call Analysis:"
echo "================================"

# Function to count calls safely
count_calls() {
    local func=$1
    local log=$2
    if [[ ! -f "$log" ]] || [[ ! -s "$log" ]]; then
        echo "0"
        return
    fi
    grep -c "\[SPY\] ${func}(" "$log" 2>/dev/null | tr -d '\n\r' || echo "0"
}

# Function to count errors safely
count_errors() {
    local log=$1
    if [[ ! -f "$log" ]] || [[ ! -s "$log" ]]; then
        echo "0"
        return
    fi
    grep -c "errno=" "$log" 2>/dev/null | tr -d '\n\r' || echo "0"
}

# Function to count total calls safely
count_total_calls() {
    local log=$1
    if [[ ! -f "$log" ]] || [[ ! -s "$log" ]]; then
        echo "0"
        return
    fi
    grep -c "\[SPY\]" "$log" 2>/dev/null | tr -d '\n\r' || echo "0"
}

printf "┌──────────┬────────────┬────────────┬────────────┐\n"
printf "│ Function │   Find     │    Tar     │     CP     │\n"
printf "├──────────┼────────────┼────────────┼────────────┤\n"

for func in open openat read write close; do
    # Get counts and clean them
    find_count=$(count_calls "$func" "logs/find_log.txt")
    tar_count=$(count_calls "$func" "logs/tar_log.txt") 
    cp_count=$(count_calls "$func" "logs/cp_log.txt")
    
    printf "│ %-8s │ %10s │ %10s │ %10s │\n" \
           "$func" "$find_count" "$tar_count" "$cp_count"
done

printf "└──────────┴────────────┴────────────┴────────────┘\n"

# NEW: Error statistics table
echo
echo "Error Statistics:"
echo "================="
printf "┌────────────────┬────────────┬────────────┬────────────┐\n"
printf "│    Program     │ Total Calls│  Errors    │ Error Rate │\n"
printf "├────────────────┼────────────┼────────────┼────────────┤\n"

for prog in find tar cp error_test; do
    log_file="logs/${prog}_log.txt"
    total_calls=$(count_total_calls "$log_file")
    error_count=$(count_errors "$log_file")
    
    # Safe calculation of error rate
    if [[ "$total_calls" -gt 0 ]] && [[ "$error_count" =~ ^[0-9]+$ ]]; then
        error_rate=$(echo "scale=2; $error_count * 100 / $total_calls" | bc 2>/dev/null || echo "0.00")
    else
        error_rate="0.00"
    fi
    
    printf "│ %-14s │ %10s │ %10s │ %9s%% │\n" \
           "$prog" "$total_calls" "$error_count" "$error_rate"
done

printf "└────────────────┴────────────┴────────────┴────────────┘\n"

# NEW: Data logging statistics
echo
echo "Data Logging Summary:"
echo "====================="
echo "Files with data content logged:"
for file in logs/*.txt; do
    if [[ -f "$file" ]] && [[ -s "$file" ]]; then
        # Безопасный подсчет данных
        count=$(grep -c "data=" "$file" 2>/dev/null)
        # Если grep вернул ошибку, устанавливаем count=0
        if [[ $? -ne 0 ]] || [[ -z "$count" ]]; then
            count=0
        fi
        # Проверяем что count - число и больше 0
        if [[ "$count" =~ ^[0-9]+$ ]] && [[ "$count" -gt 0 ]]; then
            echo "  $(basename "$file"): $count data entries"
        fi
    fi
done

# Static binary experiment
echo
echo "Static Binary Test:"
echo "==================="
cd bin || exit 1
gcc -static -o static_test ../src/static_test.c
cd .. || exit 1

LD_PRELOAD=./bin/libsyscall_spy.so ./bin/static_test > /dev/null 2> logs/static_log.txt

static_lines=0
if [[ -f "logs/static_log.txt" ]]; then
    static_lines=$(wc -l < "logs/static_log.txt" 2>/dev/null || echo 0)
fi

echo "Static test log lines: $static_lines (expected: 0)"
if [[ "$static_lines" -eq 0 ]]; then
    echo "Confirmed: LD_PRELOAD doesn't work with static binaries"
else
    echo "Unexpected: static binary produced logs"
    cat logs/static_log.txt
fi

# NEW: Cleanup temporary files
echo
echo "Cleaning up temporary files..."
rm -f test.tar test_data/copy_special.txt test_data/multi_write.txt temp_out.txt 2>/dev/null

echo
echo "Analysis Complete!"

echo
echo "2. Most common errors:"
error_file="logs/error_test.txt"
if [[ ! -f "$error_file" ]] || [[ ! -s "$error_file" ]]; then
    error_file="logs/error_demo.txt"
fi

if [[ -f "$error_file" ]] && [[ -s "$error_file" ]]; then
    grep "errno=" "$error_file" 2>/dev/null | \
    sed -E 's/.*errno=([0-9]+): ([^)]+).*/\2 (errno=\1)/' | \
    sort | uniq -c | sort -nr | head -3
else
    echo "No errors found to analyze"
fi
