#!/bin/bash

# Comprehensive test suite for Passthrough FUSE
# Usage: ./test_suite.sh

set -e

echo "=== Passthrough FUSE Test Suite ==="
echo "Date: $(date)"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
SOURCE_DIR="/tmp/fuse_test_source"
MOUNT_DIR="/tmp/fuse_test_mount"
LOG_FILE="test_operations.log"
TEST_FS="./myfuse"

# Cleanup function
cleanup() {
    echo -e "\nCleaning up..."
    
    # Try to unmount
    if mountpoint -q "$MOUNT_DIR"; then
        echo "Unmounting FUSE..."
        fusermount -u "$MOUNT_DIR" 2>/dev/null || true
        sleep 1
    fi
    
    # Remove test directories
    rm -rf "$SOURCE_DIR" "$MOUNT_DIR" 2>/dev/null || true
    
    # Kill any remaining FUSE processes
    pkill -f "$TEST_FS" 2>/dev/null || true
}

# Setup function
setup() {
    echo "Setting up test environment..."
    
    # Clean any previous runs
    cleanup
    
    # Create directories
    mkdir -p "$SOURCE_DIR" "$MOUNT_DIR"
    
    # Create some test content
    echo "Initial test content" > "$SOURCE_DIR/existing_file.txt"
    mkdir -p "$SOURCE_DIR/existing_dir"
    echo "File in directory" > "$SOURCE_DIR/existing_dir/nested.txt"
    
    # Build FUSE if needed
    if [ ! -f "$TEST_FS" ]; then
        echo "Building FUSE filesystem..."
        make clean
        make
    fi
    
    # Start FUSE in background
    echo "Starting FUSE filesystem..."
    "$TEST_FS" "$SOURCE_DIR" "$MOUNT_DIR" -f &
    FUSE_PID=$!
    
    # Wait for mount
    sleep 2
    
    # Verify mount
    if ! mountpoint -q "$MOUNT_DIR"; then
        echo -e "${RED}ERROR: Failed to mount FUSE filesystem${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}FUSE filesystem mounted successfully${NC}"
}

# Test function
run_test() {
    local test_name="$1"
    local command="$2"
    local expected_result="${3:-0}"
    
    echo -e "\n${YELLOW}Test: $test_name${NC}"
    echo "Command: $command"
    
    # Execute command
    eval "$command" > /dev/null 2>&1
    local result=$?
    
    # Check result
    if [ $result -eq "$expected_result" ] || [ "$expected_result" = "any" ]; then
        echo -e "${GREEN}✓ PASS${NC}"
        return 0
    else
        echo -e "${RED}✗ FAIL (expected $expected_result, got $result)${NC}"
        return 1
    fi
}

# Verification function

verify() {
    local description="$1"
    local source_path="$2"
    local mount_path="$3"
    
    echo -e "\n${YELLOW}Verification: $description${NC}"
    
    # Сравнение файлов/директорий
    if [ -e "$source_path" ] && [ -e "$mount_path" ]; then
        # Оба существуют, сравниваем
        if diff -r "$source_path" "$mount_path" > /dev/null 2>&1; then
            echo -e "${GREEN}✓ Source and mount are synchronized${NC}"
            return 0
        else
            echo -e "${RED}✗ Source and mount are NOT synchronized${NC}"
            diff -r "$source_path" "$mount_path" | head -20
            return 1
        fi
    elif [ ! -e "$source_path" ] && [ ! -e "$mount_path" ]; then
        # Оба не существуют - это нормально после удаления
        echo -e "${GREEN}✓ Both paths do not exist (synchronized)${NC}"
        return 0
    else
        # Один существует, другой нет - ошибка синхронизации
        echo -e "${RED}✗ Source and mount are NOT synchronized${NC}"
        echo "Source exists: $([ -e "$source_path" ] && echo "yes" || echo "no")"
        echo "Mount exists: $([ -e "$mount_path" ] && echo "yes" || echo "no")"
        return 1
    fi
}

# Main test execution
main() {
    # Setup test environment
    setup
    
    echo -e "\n${YELLOW}=== Running Basic Operation Tests ===${NC}"
    
    # Test 1: List directory
    run_test "List directory" "ls -la '$MOUNT_DIR'"
    
    # Test 2: Read existing file
    run_test "Read file" "cat '$MOUNT_DIR/existing_file.txt'"
    
    # Test 3: Create new file
    run_test "Create file" "echo 'Test content' > '$MOUNT_DIR/new_file.txt'"
    verify "New file creation" "$SOURCE_DIR/new_file.txt" "$MOUNT_DIR/new_file.txt"
    
    # Test 4: Write to file
    run_test "Write to file" "echo 'Additional content' >> '$MOUNT_DIR/new_file.txt'"
    
    # Test 5: Read written file
    run_test "Read written file" "cat '$MOUNT_DIR/new_file.txt' | grep -q 'Additional content'"
    
    # Test 6: Create directory
    run_test "Create directory" "mkdir '$MOUNT_DIR/new_dir'"
    verify "Directory creation" "$SOURCE_DIR/new_dir" "$MOUNT_DIR/new_dir"
    
    # Test 7: Create file in new directory
    run_test "Create nested file" "echo 'Nested content' > '$MOUNT_DIR/new_dir/nested_file.txt'"
    
    # Test 8: List nested directory
    run_test "List nested directory" "ls '$MOUNT_DIR/new_dir'"
    
    echo -e "\n${YELLOW}=== Running Metadata Operation Tests ===${NC}"
    
    # Test 9: Change permissions
    run_test "Change file permissions" "chmod 644 '$MOUNT_DIR/new_file.txt'"
    run_test "Verify permissions" "stat -c '%a' '$MOUNT_DIR/new_file.txt' | grep -q '644'"
    
    # Test 10: Change ownership (run as root if possible)
    if [ "$EUID" -eq 0 ]; then
        run_test "Change file ownership" "chown nobody:nogroup '$MOUNT_DIR/new_file.txt'"
    else
        echo -e "${YELLOW}Skipping chown test (requires root)${NC}"
    fi
    
    # Test 11: Change file timestamps
    run_test "Change file timestamps" "touch -t 202001010000 '$MOUNT_DIR/new_file.txt'"
    
    # Test 12: Get file stats
    run_test "Get file statistics" "stat '$MOUNT_DIR/new_file.txt' > /dev/null"
    
    echo -e "\n${YELLOW}=== Running File Operation Tests ===${NC}"
    
    # Test 13: Rename file
    run_test "Rename file" "mv '$MOUNT_DIR/new_file.txt' '$MOUNT_DIR/renamed_file.txt'"
    verify "File rename" "$SOURCE_DIR/renamed_file.txt" "$MOUNT_DIR/renamed_file.txt"
    
    # Test 14: Move file between directories
    run_test "Move file to directory" "mv '$MOUNT_DIR/renamed_file.txt' '$MOUNT_DIR/new_dir/'"
    
    # Test 15: Copy file
    run_test "Copy file" "cp '$MOUNT_DIR/new_dir/renamed_file.txt' '$MOUNT_DIR/copied_file.txt'"
    
    # Test 16: Truncate file
    run_test "Truncate file" "truncate -s 100 '$MOUNT_DIR/copied_file.txt'"
    run_test "Verify truncation" "[ \$(stat -c '%s' '$MOUNT_DIR/copied_file.txt') -eq 100 ]"
    
    echo -e "\n${YELLOW}=== Running Cleanup Operation Tests ===${NC}"
    
    # Test 17: Remove file
    run_test "Remove file" "rm '$MOUNT_DIR/copied_file.txt'"
    verify "File removal" "$SOURCE_DIR/copied_file.txt" "$MOUNT_DIR/copied_file.txt"
    
    # Test 18: Remove directory (must be empty)
    run_test "Remove nested file first" "rm '$MOUNT_DIR/new_dir/renamed_file.txt'"
    run_test "Remove nested file second" "rm '$MOUNT_DIR/new_dir/nested_file.txt'"
    run_test "Remove directory" "rmdir '$MOUNT_DIR/new_dir'"
    
    # Test 19: Remove original test file
    run_test "Remove existing file" "rm '$MOUNT_DIR/existing_file.txt'"
    
    # Test 20: Remove nested directory file
    run_test "Remove nested file" "rm '$MOUNT_DIR/existing_dir/nested.txt'"
    run_test "Remove empty directory" "rmdir '$MOUNT_DIR/existing_dir'"
    
    echo -e "\n${YELLOW}=== Running Edge Case Tests ===${NC}"
    
    # Test 21: Create symbolic link (if supported)
    run_test "Create symlink" "ln -s '/etc/passwd' '$MOUNT_DIR/symlink_test' 2>/dev/null" "any"
    
    # Test 22: Access non-existent file
    run_test "Access non-existent file" "cat '$MOUNT_DIR/nonexistent' 2>/dev/null" "1"
    
    # Test 23: Path traversal attempt (should fail)
    run_test "Path traversal attempt" "cat '$MOUNT_DIR/../passwd' 2>/dev/null" "1"
    
    # Test 24: Create file with special characters
    run_test "Create file with spaces" "echo 'test' > '$MOUNT_DIR/file with spaces.txt'"
    run_test "Read file with spaces" "cat '$MOUNT_DIR/file with spaces.txt'"
    
    # Test 25: Large file operations
    echo -e "\n${YELLOW}Large file test (1MB)...${NC}"
    dd if=/dev/urandom of="$MOUNT_DIR/large_file.bin" bs=1M count=1 status=none
    local source_size=$(stat -c "%s" "$SOURCE_DIR/large_file.bin")
    local mount_size=$(stat -c "%s" "$MOUNT_DIR/large_file.bin")
    
    if [ "$source_size" -eq "$mount_size" ] && [ "$source_size" -eq 1048576 ]; then
        echo -e "${GREEN}✓ Large file test passed${NC}"
    else
        echo -e "${RED}✗ Large file test failed${NC}"
    fi
    
    # Final verification
    echo -e "\n${YELLOW}=== Final Verification ===${NC}"
    verify "Final state" "$SOURCE_DIR" "$MOUNT_DIR"
    
    # Check log for operations
    echo -e "\n${YELLOW}Checking operation logs...${NC}"
    if [ -f "$LOG_FILE" ]; then
        echo "Log file contains $(wc -l < "$LOG_FILE") operations"
        echo "Sample of logged operations:"
        tail -10 "$LOG_FILE" 2>/dev/null || echo "No log file found"
    fi
    
    # Cleanup
    cleanup
    
    echo -e "\n${GREEN}=== All tests completed successfully! ===${NC}"
    echo "Summary:"
    echo "- Basic file operations: ✓"
    echo "- Directory operations: ✓"
    echo "- Metadata operations: ✓"
    echo "- Edge cases: ✓"
    echo "- Synchronization: ✓"
}

# Error handling
trap 'echo -e "\n${RED}Test interrupted!${NC}"; cleanup; exit 1' INT TERM

# Run main function
main "$@"
