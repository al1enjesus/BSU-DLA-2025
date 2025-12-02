obj-m += hello_module.o
obj-m += proc_module.o
obj-m += char_device.o

# Используем доступные generic заголовки
KVER = $(shell ls /lib/modules/ | grep -v WSL2 | head -1)
KDIR = /lib/modules/$(KVER)/build
PWD = $(shell pwd)

all:
	@echo "Using kernel: $(KVER)"
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load_hello:
	insmod hello_module.ko

unload_hello:
	rmmod hello_module

load_proc:
	insmod proc_module.ko

unload_proc:
	rmmod proc_module

load_char:
	insmod char_device.ko

unload_char:
	rmmod char_device

test_hello:
	dmesg -C
	insmod hello_module.ko
	dmesg | tail -5
	rmmod hello_module
	dmesg | tail -5

.PHONY: all clean
