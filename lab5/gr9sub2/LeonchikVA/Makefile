obj-m += hello_module.o
obj-m += proc_module.o
obj-m += chardev_module.o

KDIR := /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	# Удаляем временные узлы устройств, если они были созданы
	sudo rm -f /dev/mychardev

test-hello:
	@echo "=== Testing hello_module (Hello World) ==="
	@echo "Loading module without parameters..."
	sudo insmod hello_module.ko
	@echo "Unloading module..."
	sudo rmmod hello_module
	@echo "Loading module with custom message..."
	sudo insmod hello_module.ko message="Privet from kernel space!"
	@echo "Unloading module..."
	sudo rmmod hello_module
	@echo "Check dmesg for logs."


test-proc:
	@echo "=== Testing proc_module (/proc file) ==="
	@echo "Loading module..."
	sudo insmod proc_module.ko
	@echo "Checking /proc file existence and reading 3 times..."
	@ls -l /proc/student_info
	@cat /proc/student_info
	@cat /proc/student_info
	@cat /proc/student_info
	@echo "Unloading module..."
	sudo rmmod proc_module
	@echo "Check dmesg for read count (should be 3)."

test-chardev:
	@echo "=== Testing chardev_module (Character Device) ==="
	@echo "Loading module..."
	sudo insmod chardev_module.ko
	@sleep 1
	@echo "Setting permissions for /dev/mychardev..."
	# Устанавливаем права 666 для удобства, чтобы не использовать sudo cat/tee
	sudo chmod 666 /dev/mychardev 2>/dev/null || true
	@echo "Writing data to device..."
	@echo "Hello from userspace!" | tee /dev/mychardev > /dev/null
	@echo "Reading data from device..."
	@cat /dev/mychardev
	@echo "Writing and reading again..."
	@echo "New data" | tee /dev/mychardev > /dev/null
	@cat /dev/mychardev
	@echo "Unloading module..."
	sudo rmmod chardev_module

test-all: test-hello test-proc test-chardev
	@echo "=== All tests completed. Check dmesg -T for final logs. ==="

info:
	@echo "=== Modinfo for all modules ==="
	@modinfo hello_module.ko
	@modinfo proc_module.ko
	@modinfo chardev_module.ko


check:
	@echo "Loaded modules:"
	@lsmod | grep -E "hello_module|proc_module|chardev_module"

.PHONY: all clean test-hello test-proc test-chardev test-all info check