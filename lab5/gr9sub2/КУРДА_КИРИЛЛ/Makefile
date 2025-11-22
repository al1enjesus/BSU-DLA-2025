obj-m += hello_module.o proc_config.o proc_stats.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install: all
	@echo "Installing modules to /lib/modules/$(shell uname -r)/extra (requires sudo)"
	sudo mkdir -p /lib/modules/$(shell uname -r)/extra
	sudo cp -v *.ko /lib/modules/$(shell uname -r)/extra/
	sudo depmod -a

uninstall:
	@echo "Removing module files from /lib/modules/$(shell uname -r)/extra (requires sudo)"
	sudo rm -vf /lib/modules/$(shell uname -r)/extra/*.ko || true
	sudo depmod -a