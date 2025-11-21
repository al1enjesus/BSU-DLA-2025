obj-m += keylogger.o

KDIR := /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.* *.symvers *.order .*.cmd

install:
	sudo insmod keylogger.ko
	@echo "Module loaded. Check with: lsmod | grep keylogger"
	@echo "View logs with: sudo cat /proc/keylog"

uninstall:
	sudo rmmod keylogger
	@echo "Module unloaded."

dmesg:
	sudo dmesg | tail -20

log:
	sudo cat /proc/keylog

rebuild: clean all

.PHONY: all clean install uninstall dmesg log rebuild
