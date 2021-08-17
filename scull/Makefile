DEBUG = y
ifeq ($(DEBUG),y)
DEBFLAGS = -O -g -DSCULL_DEBUG
else
DEBFLAGS = -O2
endif

obj-m:= scull.o
KERNELDIR = /lib/modules/$(shell uname -r)/build
PWD:= $(shell pwd)
CFLAGS_scull.o := $(DEBFLAGS)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
clean:
	rm -rf *.ko *.depend *.mod *.order *.symvers *.mod *mod.c *.o .scull* .m* .M*


