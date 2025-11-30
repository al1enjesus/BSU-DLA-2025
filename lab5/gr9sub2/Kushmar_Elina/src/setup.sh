#!/bin/bash
echo "=== Setting up Kernel Development Environment ==="

# Update system
sudo apt update

# Install essential packages
sudo apt install -y build-essential linux-headers-$(uname -r) kmod git

# Check environment
echo "=== Environment Check ==="
echo "Kernel: $(uname -r)"
echo "Headers: $(ls -d /lib/modules/$(uname -r)/build 2>/dev/null || echo 'Not found')"
echo "GCC: $(gcc --version | head -1)"
echo "Make: $(make --version | head -1)"

# Test compilation
echo "=== Testing Compilation ==="
make clean
make

if [ $? -eq 0 ]; then
    echo "Compilation successful!"
else
    echo "Compilation failed!"
fi