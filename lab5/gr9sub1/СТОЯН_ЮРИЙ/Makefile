obj-m += hello_module.o proc_module.o char_device.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# Команды для character device
char-install:
	sudo insmod char_device.ko

char-uninstall:
	sudo rmmod char_device

char-test:
	echo "Hello from user!" > /dev/mychardev
	cat /dev/mychardev

char-logs:
	dmesg | tail -20
EOF