# Makefile for the kernel keylogger module

# obj-m means we're building a loadable module.
obj-m += keylogger.o

# Phony targets
.PHONY: all clean

# 'all' is the default target
all:
	# Run the kernel build system.
	# -C: Change directory to the kernel source tree.
	# M=$(PWD): Specifies the location of our external module source.
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

# 'clean' target to remove generated files
clean:
	# Run the kernel build system's clean command.
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean