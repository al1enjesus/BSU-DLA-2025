obj-m += hello_module.o
obj-m += proc_config.o
obj-m += sys_stats.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	sudo insmod hello_module.ko
	sudo insmod proc_config.ko
	sudo insmod sys_stats.ko

uninstall:
	sudo rmmod hello_module
	sudo rmmod proc_config
	sudo rmmod sys_stats