obj-m += hello_module.o proc_module.o char_device.o
hello_module-objs := src/hello_module.o
proc_module-objs := src/proc_module.o  
char_device-objs := src/char_device.o

KDIR := /usr/src/$(shell ls /usr/src/ | grep linux-headers | head -1)

all:
	@echo "=== Компиляция модулей ==="
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean