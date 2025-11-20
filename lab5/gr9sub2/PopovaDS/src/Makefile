PWD := $(shell pwd)
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
obj-m := hello_module.o proc_module.o chardev_module.o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement


all:
@echo "Building kernel modules..."
$(MAKE) -C $(KERNEL_DIR) M=$(PWD)/src modules
@echo "\n✓ Modules built successfully! (look in src/)"


clean:
$(MAKE) -C $(KERNEL_DIR) M=$(PWD)/src clean
rm -f src/*.o src/*.ko src/*.mod.c src/*.symvers src/*.order .tmp_versions


.PHONY: all clean


# helper targets for convenience (use from project root)
load:
if [ -z "$(MODULE)" ]; then echo "Please specify MODULE=<module_name>"; exit 1; fi
sudo insmod src/$(MODULE).ko


unload:
if [ -z "$(MODULE)" ]; then echo "Please specify MODULE=<module_name>"; exit 1; fi
sudo rmmod $(MODULE)
