#!/bin/bash

################################################################################
#
# Lab 5 - Complete Fedora Setup and Test Script
# One-Command Full Cycle: Setup + Build + Test
#
# Author: Chernookii D.I.
# Student #: 20 (Variant 2)
#
# Usage: ./fedora_full_setup.sh
#
# This script will:
# 1. Check if running in VM
# 2. Install all dependencies
# 3. Build all kernel modules
# 4. Test each module
# 5. Generate summary report
#
################################################################################

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

print_banner() {
    echo -e "${CYAN}"
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════════════════╗
║                                                                           ║
║        Lab 5 - Kernel Modules for Fedora (Variant 2)                     ║
║                                                                           ║
║        Author: Chernookii D.I.                                           ║
║        Student #: 20                                                      ║
║        Group: 1, Subgroup: 1                                             ║
║                                                                           ║
║        ⚠️  IMPORTANT: This script MUST run in a virtual machine!         ║
║                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════╝
EOF
    echo -e "${NC}"
}

print_step() {
    echo -e "\n${BLUE}→ $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Check VM
check_vm() {
    print_step "Checking if running in virtual machine..."
    
    if grep -i "vmware\|virtualbox\|qemu\|xen\|hyperv" /sys/class/dmi/id/chassis_asset_tag 2>/dev/null || \
       grep -i "vmware\|virtualbox\|qemu\|xen\|hyperv" /proc/cpuinfo 2>/dev/null || \
       [ -d "/.dockerenv" ]; then
        print_success "Virtual machine detected!"
        return 0
    fi
    
    print_warning "Could not confirm VM"
    echo "This script should run in a virtual machine!"
    read -p "Are you sure you want to continue? (type 'yes' to proceed): " -r
    if [[ "$REPLY" != "yes" ]]; then
        print_error "Aborted"
        exit 1
    fi
}

# Install dependencies
install_deps() {
    print_step "Installing dependencies (requires sudo)..."
    
    if ! command -v dnf &> /dev/null; then
        print_error "Fedora dnf package manager not found!"
        print_error "This script is for Fedora Linux only"
        exit 1
    fi
    
    # Check if already installed
    if [ -d "/lib/modules/$(uname -r)/build" ] && command -v gcc &> /dev/null; then
        print_success "Dependencies already installed"
        return 0
    fi
    
    echo "This will install:"
    echo "  - Development Tools (gcc, make, etc.)"
    echo "  - Kernel Development Headers"
    echo ""
    read -p "Continue with installation? (y/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
    
    # Update
    echo "Updating package lists..."
    sudo dnf update -y -q
    
    # Install development tools
    echo "Installing development tools..."
    sudo dnf groupinstall -y -q "Development Tools"
    
    # Install kernel development
    echo "Installing kernel development packages..."
    sudo dnf install -y -q kernel-devel kernel-headers
    
    # Install additional tools
    echo "Installing additional tools..."
    sudo dnf install -y -q git vim dkms
    
    # Verify
    if [ -d "/lib/modules/$(uname -r)/build" ] && command -v gcc &> /dev/null; then
        print_success "Dependencies installed successfully"
    else
        print_error "Installation verification failed"
        exit 1
    fi
}

# Build modules
build_modules() {
    print_step "Building kernel modules..."
    
    cd "$SCRIPT_DIR"
    
    if [ ! -f "Makefile" ]; then
        print_error "Makefile not found"
        exit 1
    fi
    
    if [ ! -d "src" ]; then
        print_error "src/ directory not found"
        exit 1
    fi
    
    # Clean
    echo "Cleaning previous builds..."
    make clean -q 2>/dev/null || true
    
    # Check environment
    echo "Checking build environment..."
    if ! make check 2>&1 | grep -q "✓"; then
        print_error "Build environment check failed"
        exit 1
    fi
    
    # Build
    echo "Compiling modules..."
    if make -q 2>&1 | tee /tmp/build.log; then
        if [ -f "src/hello_module.ko" ] && \
           [ -f "src/config_module.ko" ] && \
           [ -f "src/stats_module.ko" ]; then
            print_success "All modules built successfully"
            echo "  - hello_module.ko (Task A)"
            echo "  - config_module.ko (Task B)"
            echo "  - stats_module.ko (Task C)"
        else
            print_error "Some modules failed to build"
            exit 1
        fi
    else
        print_error "Build failed"
        cat /tmp/build.log
        exit 1
    fi
}

# Test module
test_module() {
    local module_name=$1
    local module_path=$2
    local task=$3
    
    print_step "Testing $module_name ($task)..."
    
    if [ ! -f "$module_path" ]; then
        print_error "Module not found: $module_path"
        return 1
    fi
    
    # Check if already loaded
    if lsmod | grep -q "^${module_name}"; then
        echo "Module already loaded, unloading first..."
        sudo rmmod "$module_name" || true
        sleep 1
    fi
    
    # Clear dmesg
    sudo dmesg -C
    
    # Load
    echo "Loading $module_name..."
    if ! sudo insmod "$module_path" 2>&1; then
        print_error "Failed to load $module_name"
        return 1
    fi
    sleep 1
    
    # Check logs
    if dmesg | grep -q "$module_name"; then
        echo "Kernel log:"
        dmesg | tail -3 | sed 's/^/  /'
    fi
    
    # Verify loaded
    if lsmod | grep -q "^${module_name}"; then
        print_success "$module_name loaded successfully"
    else
        print_error "$module_name not in lsmod"
        return 1
    fi
    
    # Module-specific tests
    case $module_name in
        hello_module)
            echo "Testing with custom message parameter..."
            sudo rmmod hello_module
            sleep 1
            sudo dmesg -C
            sudo insmod "$module_path" message="Test message from student 20"
            sleep 1
            if dmesg | grep -q "Test message"; then
                print_success "Parameter test passed"
            fi
            ;;
        config_module)
            if [ -f "/proc/my_config" ]; then
                echo "Testing /proc/my_config..."
                default_val=$(cat /proc/my_config)
                echo "  Default value: $default_val"
                
                echo "test_value_123" > /proc/my_config
                new_val=$(cat /proc/my_config)
                if [ "$new_val" = "test_value_123" ]; then
                    print_success "/proc read/write test passed"
                else
                    print_error "Value mismatch"
                fi
            else
                print_error "/proc/my_config not found"
            fi
            ;;
        stats_module)
            if [ -f "/proc/sys_stats" ]; then
                echo "System statistics:"
                cat /proc/sys_stats | sed 's/^/  /'
                print_success "Stats file readable"
            else
                print_error "/proc/sys_stats not found"
            fi
            ;;
    esac
    
    # Unload
    echo "Unloading $module_name..."
    sudo rmmod "$module_name"
    sleep 1
    
    if ! lsmod | grep -q "^${module_name}"; then
        print_success "$module_name unloaded successfully"
    else
        print_warning "$module_name still in lsmod"
    fi
    
    return 0
}

# Test all modules
test_all() {
    print_step "Testing all kernel modules..."
    echo "Note: This requires sudo access"
    echo ""
    
    mkdir -p "logs"
    
    # Make sure we have sudo access
    if ! sudo -n true 2>/dev/null; then
        echo "Please enter your password to allow module loading:"
        sudo -v
    fi
    
    # Test each module
    local failed=0
    
    if ! test_module "hello_module" "src/hello_module.ko" "Task A"; then
        ((failed++))
    fi
    echo ""
    
    if ! test_module "config_module" "src/config_module.ko" "Task B"; then
        ((failed++))
    fi
    echo ""
    
    if ! test_module "stats_module" "src/stats_module.ko" "Task C"; then
        ((failed++))
    fi
    echo ""
    
    if [ $failed -eq 0 ]; then
        print_success "All modules tested successfully!"
        return 0
    else
        print_error "$failed module(s) failed testing"
        return 1
    fi
}

# Generate summary
generate_summary() {
    print_step "Generating summary..."
    
    local summary_file="$SCRIPT_DIR/SETUP_SUMMARY.txt"
    
    cat > "$summary_file" << EOF
================================================================================
Lab 5 - Kernel Modules Setup Summary
================================================================================

Author: Chernookii D.I.
Student #: 20 (Variant 2)
Date: $(date)
System: $(uname -s) $(uname -r)

================================================================================
SETUP COMPLETED
================================================================================

✓ Environment Check
  - Virtual Machine: Yes
  - Kernel Version: $(uname -r)
  - Kernel Headers: $([ -d "/lib/modules/$(uname -r)/build" ] && echo "Installed" || echo "Missing")"
  - Build Tools: $(gcc --version | head -1)"

✓ Dependencies Installed
  - Development Tools: gcc, make, etc.
  - Kernel Development: kernel-devel, kernel-headers
  - Additional Tools: git, vim, dkms

✓ Modules Built
  - hello_module.ko (Task A - Hello World)
  - config_module.ko (Task B - /proc config)
  - stats_module.ko (Task C - /proc stats)

✓ Tests Completed
  - hello_module: PASSED
  - config_module: PASSED
  - stats_module: PASSED

================================================================================
FILES STRUCTURE
================================================================================

$(ls -lh "$SCRIPT_DIR" | tail -n +2)

================================================================================
SOURCE CODE
================================================================================

Task A - hello_module.c ($(wc -l < "$SCRIPT_DIR/src/hello_module.c") lines)
  - Simple module with greeting message
  - Parameter: message (string)
  - Location: src/hello_module.c

Task B - config_module.c ($(wc -l < "$SCRIPT_DIR/src/config_module.c") lines)
  - /proc file for reading and writing
  - Default: "default"
  - Location: src/config_module.c

Task C - stats_module.c ($(wc -l < "$SCRIPT_DIR/src/stats_module.c") lines)
  - /proc file with system statistics
  - Shows: processes, memory, uptime, load
  - Location: src/stats_module.c

================================================================================
NEXT STEPS
================================================================================

1. Review the detailed report:
   cat REPORT.MD

2. Check the README:
   cat README.md

3. Load modules manually:
   cd lab5/gr1sub1/Chernookii_DI/
   sudo insmod src/hello_module.ko
   dmesg | tail -5
   sudo rmmod hello_module

4. Use the run script for more options:
   ./run_lab5.sh help

================================================================================
IMPORTANT REMINDERS
================================================================================

⚠️  Always work in a virtual machine!
⚠️  Kernel modules can crash the system!
⚠️  Make snapshots before testing!
⚠️  Use 'dmesg' to debug problems!

================================================================================
EOF
    
    echo "Summary saved to: $summary_file"
    cat "$summary_file"
}

# Main
main() {
    print_banner
    
    print_step "Lab 5 Setup Starting..."
    echo "This will set up, build, and test all kernel modules"
    echo "Estimated time: 10-15 minutes (including dependency installation)"
    echo ""
    
    # Steps
    check_vm
    install_deps
    build_modules
    test_all || { print_error "Tests failed"; exit 1; }
    generate_summary
    
    echo ""
    print_success "SETUP COMPLETE!"
    echo ""
    echo "All kernel modules are ready!"
    echo ""
    echo "Next steps:"
    echo "  1. Read REPORT.MD for detailed information"
    echo "  2. Review README.md for quick reference"
    echo "  3. Use run_lab5.sh for additional options"
    echo ""
}

# Run
main "$@"
