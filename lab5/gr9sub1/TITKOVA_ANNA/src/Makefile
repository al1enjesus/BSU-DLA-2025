# Простой Makefile для сборки модулей ядра
# Использование:
#   make                - Собрать все модули
#   make hello          - Собрать только hello_module
#   make proc           - Собрать только proc_module
#   make chardev        - Собрать только chardev_module
#   make clean          - Очистить всё

obj-m += hello_module.o
obj-m += proc_module.o
obj-m += chardev_module.o

PWD := $(shell pwd)
KERNEL_DIR := /lib/modules/$(shell uname -r)/build

all: hello proc chardev

hello:
	@echo "Building hello_module..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) hello_module.ko

proc:
	@echo "Building proc_module..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) proc_module.ko

chardev:
	@echo "Building chardev_module..."
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) chardev_module.ko

clean:
	@echo "Cleaning up..."
	rm -f *.o *.ko *.mod.c *.mod *.symvers *.order .*.cmd
	rm -rf .tmp_versions
	@echo "Cleanup complete"

.PHONY: all hello proc chardev clean
