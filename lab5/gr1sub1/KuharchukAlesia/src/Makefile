obj-m += hello_module.o
obj-m += proc_module.o
obj-m += char_device.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

check:
	@echo "=== Environment Check ==="
	@echo "Kernel version: $(shell uname -r)"
	@if [ -d "$(KDIR)" ]; then \
		echo "Kernel headers: ✓ Found at $(KDIR)"; \
	else \
		echo "Kernel headers: ✗ Not found!"; \
	fi
	@which gcc > /dev/null && echo "Build tools: ✓ gcc installed" || echo "Build tools: ✗ gcc not found"
	@which insmod > /dev/null && echo "Module loading: ✓ insmod available" || echo "Module loading: ✗ insmod not found"
