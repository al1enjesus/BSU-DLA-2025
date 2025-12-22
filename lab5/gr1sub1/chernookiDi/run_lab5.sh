#!/bin/bash

################################################################################
#
# Lab 5 - Kernel Modules Setup and Run Script for Fedora
# Author: Chernookii D.I.
# Student #: 20 (Variant 2)
# Group: 1, Subgroup: 1
#
# This script sets up the environment and runs all kernel modules
# for Lab 5 in a Fedora virtual machine.
#
# IMPORTANT: This script MUST be run in a virtual machine!
#            Kernel modules can crash the system!
#
# Usage:
#   ./run_lab5.sh [option]
#
# Options:
#   help              - Show this help message
#   setup             - Setup environment (install dependencies)
#   build             - Build all modules
#   test-hello        - Test hello_module.ko
#   test-config       - Test config_module.ko
#   test-stats        - Test stats_module.ko
#   test-all          - Test all modules
#   clean             - Clean compiled modules
#   full-cycle        - Setup, build, and test all
#
################################################################################

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script variables
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="$SCRIPT_DIR"
SRC_DIR="$WORK_DIR/src"
LOG_DIR="$WORK_DIR/logs"
SCREENSHOT_DIR="$WORK_DIR/screenshots"

# Check if running as root for some operations
need_root() {
    if [[ $EUID -ne 0 ]]; then
        echo -e "${RED}This operation requires root privileges.${NC}"
        echo "Please run: sudo $0 $1"
        exit 1
    fi
}

# Print section header
print_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

# Print success message
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Print warning message
print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Print error message
print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# Show help
show_help() {
    cat << EOF
Lab 5 - Kernel Modules - Fedora Setup Script

Author: Chernookii D.I.
Student #: 20 (Variant 2)

⚠️  WARNING: This script works with kernel modules!
    It MUST be run in a virtual machine!

USAGE:
    $0 [option]

OPTIONS:
    help              - Show this help message
    setup             - Install dependencies (kernel-devel, build-essential)
    build             - Build all kernel modules
    test-hello        - Load and test hello_module.ko
    test-config       - Load and test config_module.ko
    test-stats        - Load and test stats_module.ko
    test-all          - Test all modules sequentially
    clean             - Clean up compiled modules and object files
    full-cycle        - Complete setup: setup -> build -> test-all

EXAMPLES:
    # First time setup
    $0 setup

    # Build modules
    $0 build

    # Test individual module
    $0 test-hello

    # Test all modules
    $0 test-all

    # Complete cycle
    $0 full-cycle

    # Cleanup
    $0 clean

INSIDE VIRTUAL MACHINE:
    1. Open terminal
    2. Navigate to lab5/gr1sub1/Chernookii_DI/
    3. Run: chmod +x run_lab5.sh
    4. Run: ./run_lab5.sh setup
    5. Run: ./run_lab5.sh build
    6. Run: sudo ./run_lab5.sh test-all

EOF
}

# Check environment
check_environment() {
    print_header "Checking Environment"

    echo "Kernel version:"
    uname -r

    echo ""
    echo "Checking kernel headers..."
    if [ -d "/lib/modules/$(uname -r)/build" ]; then
        print_success "Kernel headers found"
    else
        print_error "Kernel headers NOT found"
        echo "Run: $0 setup"
        exit 1
    fi

    echo ""
    echo "Checking build tools..."
    if command -v gcc &> /dev/null; then
        print_success "GCC found: $(gcc --version | head -1)"
    else
        print_error "GCC NOT found"
        echo "Run: $0 setup"
        exit 1
    fi

    echo ""
    if command -v make &> /dev/null; then
        print_success "Make found: $(make --version | head -1)"
    else
        print_error "Make NOT found"
        echo "Run: $0 setup"
        exit 1
    fi

    echo ""
    print_success "Environment check passed!"
}

# Setup environment (install dependencies)
setup_environment() {
    need_root "setup"
    
    print_header "Setting Up Environment"

    echo "This will install kernel development packages."
    echo "Requires internet connection and sudo privileges."
    echo ""
    read -p "Continue? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi

    echo ""
    echo "Updating package lists..."
    dnf update -y

    echo ""
    echo "Installing Development Tools..."
    dnf groupinstall -y "Development Tools" "Fedora Workstation"

    echo ""
    echo "Installing kernel-devel and kernel-headers..."
    dnf install -y kernel-devel kernel-headers

    echo ""
    echo "Installing additional tools..."
    dnf install -y git vim dkms

    echo ""
    print_success "Environment setup complete!"

    echo ""
    echo "Verifying installation..."
    check_environment
}

# Build all modules
build_modules() {
    print_header "Building Kernel Modules"

    if [ ! -f "$WORK_DIR/Makefile" ]; then
        print_error "Makefile not found at $WORK_DIR/Makefile"
        exit 1
    fi

    echo "Checking environment..."
    check_environment

    echo ""
    echo "Building modules..."
    cd "$WORK_DIR"
    make clean
    make

    echo ""
    echo "Build artifacts:"
    ls -lh src/*.ko 2>/dev/null || true

    print_success "Build completed!"
}

# Test hello_module
test_hello_module() {
    need_root "test-hello"

    print_header "Testing hello_module.ko (Task A)"

    if [ ! -f "$SRC_DIR/hello_module.ko" ]; then
        print_error "Module not found. Run: $0 build"
        exit 1
    fi

    mkdir -p "$LOG_DIR"
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    LOG_FILE="$LOG_DIR/hello_test_$TIMESTAMP.log"

    echo "Test 1: Loading without parameters..."
    dmesg -C  # Clear dmesg
    insmod "$SRC_DIR/hello_module.ko" | tee "$LOG_FILE"
    sleep 1
    echo ""
    echo "Kernel messages:"
    dmesg | tail -3 | tee -a "$LOG_FILE"
    sleep 1

    echo ""
    echo "Checking module is loaded..."
    lsmod | grep hello_module | tee -a "$LOG_FILE"

    echo ""
    echo "Unloading module..."
    rmmod hello_module | tee -a "$LOG_FILE"
    sleep 1
    echo ""
    echo "Kernel messages after unload:"
    dmesg | tail -1 | tee -a "$LOG_FILE"

    echo ""
    echo "Test 2: Loading with parameter..."
    dmesg -C
    insmod "$SRC_DIR/hello_module.ko" message="Custom greeting from student #20" | tee -a "$LOG_FILE"
    sleep 1
    echo ""
    echo "Kernel messages:"
    dmesg | tail -1 | tee -a "$LOG_FILE"

    echo ""
    echo "Unloading..."
    rmmod hello_module | tee -a "$LOG_FILE"

    print_success "hello_module tests completed! Log: $LOG_FILE"
}

# Test config_module
test_config_module() {
    need_root "test-config"

    print_header "Testing config_module.ko (Task B)"

    if [ ! -f "$SRC_DIR/config_module.ko" ]; then
        print_error "Module not found. Run: $0 build"
        exit 1
    fi

    mkdir -p "$LOG_DIR"
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    LOG_FILE="$LOG_DIR/config_test_$TIMESTAMP.log"

    echo "Loading module..."
    dmesg -C
    insmod "$SRC_DIR/config_module.ko" | tee "$LOG_FILE"
    sleep 2

    echo ""
    echo "Checking /proc/my_config file..."
    if [ -f "/proc/my_config" ]; then
        print_success "/proc/my_config created"
    else
        print_error "/proc/my_config NOT found"
        rmmod config_module
        exit 1
    fi

    echo ""
    echo "Test 1: Reading default value..."
    cat /proc/my_config | tee "$LOG_FILE"

    echo ""
    echo "Test 2: Writing new value..."
    echo "test_configuration_value" > /proc/my_config
    echo "Written: test_configuration_value" | tee -a "$LOG_FILE"

    echo ""
    echo "Test 3: Reading updated value..."
    cat /proc/my_config | tee -a "$LOG_FILE"

    echo ""
    echo "Test 4: Writing another value..."
    echo "Student 20 - Configuration Test" > /proc/my_config
    cat /proc/my_config | tee -a "$LOG_FILE"

    echo ""
    echo "File permissions:"
    ls -la /proc/my_config | tee -a "$LOG_FILE"

    echo ""
    echo "Kernel messages:"
    dmesg | grep config_module | tee -a "$LOG_FILE"

    echo ""
    echo "Unloading module..."
    rmmod config_module

    print_success "config_module tests completed! Log: $LOG_FILE"
}

# Test stats_module
test_stats_module() {
    need_root "test-stats"

    print_header "Testing stats_module.ko (Task C)"

    if [ ! -f "$SRC_DIR/stats_module.ko" ]; then
        print_error "Module not found. Run: $0 build"
        exit 1
    fi

    mkdir -p "$LOG_DIR"
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    LOG_FILE="$LOG_DIR/stats_test_$TIMESTAMP.log"

    echo "Loading module..."
    dmesg -C
    insmod "$SRC_DIR/stats_module.ko" | tee "$LOG_FILE"
    sleep 2

    echo ""
    echo "Checking /proc/sys_stats file..."
    if [ -f "/proc/sys_stats" ]; then
        print_success "/proc/sys_stats created"
    else
        print_error "/proc/sys_stats NOT found"
        rmmod stats_module
        exit 1
    fi

    echo ""
    echo "Test 1: Reading system statistics (first read)..."
    cat /proc/sys_stats | tee "$LOG_FILE"

    echo ""
    echo "Test 2: Reading statistics after 2 seconds..."
    sleep 2
    cat /proc/sys_stats | tee -a "$LOG_FILE"

    echo ""
    echo "Test 3: File permissions..."
    ls -la /proc/sys_stats | tee -a "$LOG_FILE"

    echo ""
    echo "Kernel messages:"
    dmesg | grep stats_module | tee -a "$LOG_FILE"

    echo ""
    echo "Unloading module..."
    rmmod stats_module

    print_success "stats_module tests completed! Log: $LOG_FILE"
}

# Test all modules
test_all_modules() {
    print_header "Testing All Modules"

    echo "This will test all three kernel modules sequentially."
    echo ""

    if ! test_hello_module; then
        print_error "hello_module test failed"
        return 1
    fi

    echo ""
    echo ""

    if ! test_config_module; then
        print_error "config_module test failed"
        return 1
    fi

    echo ""
    echo ""

    if ! test_stats_module; then
        print_error "stats_module test failed"
        return 1
    fi

    echo ""
    print_success "All tests completed!"
    
    # Show summary
    print_header "Test Summary"
    echo "✓ hello_module.ko - Hello World with parameter"
    echo "✓ config_module.ko - /proc file with read/write"
    echo "✓ stats_module.ko - /proc file with system statistics"
    echo ""
    echo "Logs saved in: $LOG_DIR"
    ls -lh "$LOG_DIR"/*.log 2>/dev/null || echo "No log files"
}

# Clean up
clean_modules() {
    print_header "Cleaning Up"

    if [ ! -f "$WORK_DIR/Makefile" ]; then
        print_error "Makefile not found"
        exit 1
    fi

    cd "$WORK_DIR"
    make clean

    echo ""
    echo "Checking for leftover files..."
    find . -name "*.ko" -o -name "*.o" -o -name "*.mod.c" | while read file; do
        echo "  Removing: $file"
        rm -f "$file"
    done

    print_success "Cleanup completed!"
}

# Full cycle: setup -> build -> test
full_cycle() {
    print_header "Full Cycle: Setup -> Build -> Test"

    echo "This will:"
    echo "  1. Setup environment (install dependencies)"
    echo "  2. Build all kernel modules"
    echo "  3. Test all modules"
    echo ""
    read -p "Continue? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi

    if ! setup_environment; then
        print_error "Setup failed"
        exit 1
    fi

    echo ""
    if ! build_modules; then
        print_error "Build failed"
        exit 1
    fi

    echo ""
    if ! sudo "$0" test-all; then
        print_error "Tests failed"
        exit 1
    fi

    print_success "Full cycle completed!"
}

# Main script logic
main() {
    # Check if running in virtual machine
    if [ -z "$VIRTUAL_MACHINE" ] && [ ! -f "/.dockerenv" ]; then
        print_warning "This script should be run in a virtual machine"
        print_warning "Running kernel modules on host system is DANGEROUS"
        read -p "Are you running this in a virtual machine? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_error "Script aborted. Please run in a virtual machine."
            exit 1
        fi
    fi

    case "${1:-help}" in
        help)
            show_help
            ;;
        setup)
            setup_environment
            ;;
        build)
            build_modules
            ;;
        test-hello)
            test_hello_module
            ;;
        test-config)
            test_config_module
            ;;
        test-stats)
            test_stats_module
            ;;
        test-all)
            test_all_modules
            ;;
        clean)
            clean_modules
            ;;
        full-cycle)
            full_cycle
            ;;
        *)
            print_error "Unknown option: $1"
            echo ""
            show_help
            exit 1
            ;;
    esac
}

# Run main
main "$@"
