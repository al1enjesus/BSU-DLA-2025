PWD := $(shell pwd)

KERNEL_DIR := /lib/modules/$(shell uname -r)/build

SRC_DIR := src

obj-m += hello_module.o
obj-m += proc_module.o
obj-m += chardev_module.o

hello_module-objs := $(SRC_DIR)/hello_module.o
proc_module-objs := $(SRC_DIR)/proc_module.o
chardev_module-objs := $(SRC_DIR)/chardev_module.o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement

all:
	@echo "Building kernel modules from $(SRC_DIR)/..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
	@echo ""
	@echo "✓ Modules built successfully!"
	@echo "  To load: sudo insmod <module>.ko"
	@echo "  To view logs: dmesg | tail"
	@echo ""

clean:
	@echo "Cleaning up..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c *.mod *.symvers *.order .*.cmd
	rm -rf .tmp_versions
	@echo "✓ Cleanup complete"

load:
	@if [ -z "$(MODULE)" ]; then \
		echo "Error: Please specify MODULE=<module_name>"; \
		echo "Example: make load MODULE=hello_module"; \
		exit 1; \
	fi
	@if [ ! -f $(MODULE).ko ]; then \
		echo "Error: $(MODULE).ko not found. Run 'make' first."; \
		exit 1; \
	fi
	@echo "Loading $(MODULE).ko..."
	sudo insmod $(MODULE).ko
	@echo "✓ Module loaded. Check 'dmesg | tail' for output"

unload:
	@if [ -z "$(MODULE)" ]; then \
		echo "Error: Please specify MODULE=<module_name>"; \
		echo "Example: make unload MODULE=hello_module"; \
		exit 1; \
	fi
	@echo "Unloading $(MODULE)..."
	sudo rmmod $(MODULE)
	@echo "✓ Module unloaded. Check 'dmesg | tail' for output"

info:
	@echo "=== Loaded Modules ==="
	@lsmod | head -1
	@lsmod | grep -E "(hello_module|proc_module|chardev_module)" || echo "No lab5 modules loaded"
	@echo ""
	@echo "=== Built Modules ==="
	@ls -lh *.ko 2>/dev/null || echo "No modules built yet (run 'make')"

test-hello: all
	@echo "=== Testing hello_module ==="
	@echo "Loading module..."
	-@sudo rmmod hello_module 2>/dev/null || true
	sudo insmod hello_module.ko
	@echo ""
	@echo "Output:"
	sudo dmesg | tail -5
	@echo ""
	@echo "Unloading module..."
	sudo rmmod hello_module
	@echo ""
	@echo "Output:"
	sudo dmesg | tail -5

test-proc: all
	@echo "=== Testing proc_module ==="
	@echo "Loading module..."
	-@sudo rmmod proc_module 2>/dev/null || true
	sudo insmod proc_module.ko
	@echo ""
	@echo "Reading /proc/student_info:"
	@cat /proc/student_info 2>/dev/null || echo "File not created (check dmesg)"
	@echo ""
	@echo "Unloading module..."
	sudo rmmod proc_module
	@echo ""
	@echo "dmesg output:"
	sudo dmesg | tail -5

test-chardev: all
	@echo "=== Testing chardev_module ==="
	@echo "Loading module..."
	-@sudo rmmod chardev_module 2>/dev/null || true
	-@sudo rm /dev/mychardev 2>/dev/null || true
	sudo insmod chardev_module.ko
	@echo ""
	@MAJOR=$$(sudo dmesg | grep "chardev: Registered" | tail -1 | grep -o "number [0-9]*" | awk '{print $$2}'); \
	if [ -n "$$MAJOR" ]; then \
		echo "Creating device node with major $$MAJOR..."; \
		sudo mknod /dev/mychardev c $$MAJOR 0; \
		sudo chmod 666 /dev/mychardev; \
		echo "Writing to device..."; \
		echo "Test data" > /dev/mychardev; \
		echo "Reading from device:"; \
		cat /dev/mychardev; \
		echo ""; \
		sudo rm /dev/mychardev; \
	else \
		echo "Failed to get major number (check dmesg)"; \
	fi
	@echo ""
	@echo "Unloading module..."
	sudo rmmod chardev_module
	@echo ""
	@echo "dmesg output:"
	sudo dmesg | tail -10

check:
	@echo "=== Environment Check ==="
	@echo -n "Kernel version: "
	@uname -r
	@echo -n "Kernel headers: "
	@if [ -d "$(KERNEL_DIR)" ]; then \
		echo "✓ Found at $(KERNEL_DIR)"; \
	else \
		echo "✗ NOT FOUND"; \
		echo "  Install with: sudo apt install linux-headers-$$(uname -r)"; \
	fi
	@echo -n "Build tools: "
	@which gcc > /dev/null && echo "✓ gcc installed" || echo "✗ gcc NOT installed"
	@echo -n "Module loading: "
	@which insmod > /dev/null && echo "✓ insmod available" || echo "✗ insmod NOT available"
	@echo ""
	@echo "Kernel module support:"
	@zcat /proc/config.gz 2>/dev/null | grep -E "CONFIG_MODULES|CONFIG_MODULE" | head -5 || \
	 cat /boot/config-$$(uname -r) 2>/dev/null | grep -E "CONFIG_MODULES|CONFIG_MODULE" | head -5 || \
	 echo "  (Unable to check kernel config)"

help:
	@echo "Lab 5 Kernel Modules - Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  make              - Build all modules"
	@echo "  make clean        - Remove built modules and temporary files"
	@echo "  make check        - Check environment (kernel headers, tools)"
	@echo "  make info         - Show loaded and built modules"
	@echo ""
	@echo "Module operations:"
	@echo "  make load MODULE=<name>    - Load a module"
	@echo "  make unload MODULE=<name>  - Unload a module"
	@echo ""
	@echo "Quick tests:"
	@echo "  make test-hello    - Test hello_module"
	@echo "  make test-proc     - Test proc_module"
	@echo "  make test-chardev  - Test chardev_module"
	@echo ""
	@echo "Examples:"
	@echo "  make                           # Build everything"
	@echo "  make load MODULE=hello_module  # Load hello module"
	@echo "  make test-proc                 # Test proc module"
	@echo "  make unload MODULE=proc_module # Unload proc module"
	@echo ""
	@echo "Manual operations:"
	@echo "  sudo insmod hello_module.ko                  # Load module"
	@echo "  sudo insmod hello_module.ko message=\"Test\"  # Load with parameter"
	@echo "  lsmod | grep hello_module                    # Check if loaded"
	@echo "  dmesg | tail                                 # View kernel logs"
	@echo "  sudo rmmod hello_module                      # Unload module"

.PHONY: all clean load unload info test-hello test-proc test-chardev check help
