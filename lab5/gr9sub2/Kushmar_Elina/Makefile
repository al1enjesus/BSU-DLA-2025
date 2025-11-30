obj-m += hello_module.o
obj-m += proc_module.o  
obj-m += char_device.o

# Auto-detect kernel build directory
KERNEL_VERSION := $(shell uname -r)
KDIR ?= /lib/modules/$(KERNEL_VERSION)/build
PWD := $(shell pwd)

# Default target
default:
	@echo "Building kernel modules for $(KERNEL_VERSION)"
	@if [ ! -d "$(KDIR)" ]; then \
		echo "Error: Kernel headers not found at $(KDIR)"; \
		echo "Please install: sudo apt install linux-headers-$(KERNEL_VERSION)"; \
		exit 1; \
	fi
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean build files
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f modules.order Module.symvers

# Check development environment
check:
	@echo "=== Kernel Module Development Environment ==="
	@echo "Kernel version: $(KERNEL_VERSION)"
	@echo "Build directory: $(KDIR)"
	@if [ -d "$(KDIR)" ]; then \
		echo "Kernel headers: INSTALLED"; \
	else \
		echo "Kernel headers: MISSING"; \
		echo "  Install with: sudo apt install linux-headers-$(KERNEL_VERSION)"; \
	fi
	@which gcc > /dev/null && echo "GCC compiler: INSTALLED" || echo "GCC compiler: MISSING"
	@which make > /dev/null && echo "Make tool: INSTALLED" || echo "Make tool: MISSING"
	@which insmod > /dev/null && echo "Module tools: INSTALLED" || echo "Module tools: MISSING"

# Install and test all modules
test: default
	@echo "=== Testing Modules ==="
	-sudo rmmod char_device 2>/dev/null || true
	-sudo rmmod proc_module 2>/dev/null || true  
	-sudo rmmod hello_module 2>/dev/null || true
	
	@echo "1. Testing hello_module..."
	sudo insmod hello_module.ko
	sudo dmesg | grep hello_module | tail -1
	sudo rmmod hello_module
	sudo dmesg | grep hello_module | tail -1
	
	@echo "2. Testing proc_module..."
	sudo insmod proc_module.ko
	cat /proc/student_info 2>/dev/null && echo "/proc/student_info created" || echo "/proc/student_info failed"
	sudo rmmod proc_module
	
	@echo "3. Testing char_device..."
	sudo insmod char_device.ko
	MAJOR=$$(dmesg | grep "Registered with major number" | tail -1 | awk '{print $$NF}'); \
	if [ -n "$$MAJOR" ]; then \
		echo "char_device registered with major $$MAJOR"; \
	else \
		echo "char_device registration failed"; \
	fi
	sudo rmmod char_device

.PHONY: default clean check test