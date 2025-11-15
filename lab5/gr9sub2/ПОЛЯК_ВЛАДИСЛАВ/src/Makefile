obj-m += hello_world_module.o 
obj-m += proc_module.o
obj-m += system_info_module.o

PWD :=$(CURDIR)

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules  
    
clean: 
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
