obj-m += hello_module.o
obj-m += proc_config.o
obj-m += sys_stats.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

test:
	sudo ./run.sh

.PHONY: all clean test