# Kernel module specific part
obj-m := keylogger.o

# Get kernel version
KERNEL_VERSION := $(shell uname -r)

# Set kernel directory
KDIR := /lib/modules/$(KERNEL_VERSION)/build

# Set project directory
PWD := $(shell pwd)

# Specify the compiler
# CC ?= x86_64-linux-gnu-gcc-13

# Main target
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean target
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Check environment target
check:
	@echo "=== Environment Check ==="
	@echo "Kernel version: $(KERNEL_VERSION)"
	@if [ -d "$(KDIR)" ]; then \
		echo "Kernel headers: ✓ Found at $(KDIR)"; \
	else \
		echo "Kernel headers: ✗ Not found, please install linux-headers-$(KERNEL_VERSION)"; \
		exit 1; \
	fi
	@if [ -x "$(shell which gcc)" ]; then \
		echo "Build tools: ✓ gcc installed"; \
	else \
		echo "Build tools: ✗ gcc not found, please install build-essential"; \
		exit 1; \
	fi
	@if [ -x "$(shell which insmod)" ]; then \
		echo "Module loading: ✓ insmod available"; \
	else \
		echo "Module loading: ✗ kmod package not found, please install kmod"; \
		exit 1; \
	fi

# Phony targets
.PHONY: all clean check