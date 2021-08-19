DEBUG = y

ifeq ($(DEBUG),y)
	DEBFLAGS = -O -g -DSCULL_DEBUG
else
	DEBFLAGS = -O2
endif

CFLAGS_scullp.o += $(DEBFLAGS)
INCLUDE_DIR := $(PWD)/../include/
EXTRA_CFLAGS += -I$(INCLUDE_DIR)

obj-m:= scullp.o

KERNELDIR = /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.ko *.depend *.mod *.order *.symvers *.mod *mod.c *.o .scull* .m* .M*


