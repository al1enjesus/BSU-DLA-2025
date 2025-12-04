obj-m += hello_module.o
obj-m += proc_module.o
obj-m += char_device.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install-char:
	sudo insmod char_device.ko

uninstall-char:
	sudo rmmod char_device
