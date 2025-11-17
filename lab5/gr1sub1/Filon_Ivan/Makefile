KDIR := /lib/modules/$(shell uname -r)/build
SRC := $(shell pwd)/src

all:
	$(MAKE) -C $(KDIR) M=$(SRC) modules

clean:
	$(MAKE) -C $(KDIR) M=$(SRC) clean
