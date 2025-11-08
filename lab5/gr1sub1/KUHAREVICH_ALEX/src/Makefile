CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
obj-m := hello_module.o proc_config.o proc_stats.o
CFLAGS_MODULE := -g -Wall
all:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f *.ko
