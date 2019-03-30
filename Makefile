TARGET = w5x00.ko

#MDIR = arch/arm/w5x00
MDIR = drivers/net/ethernet/wiznet

DRIVER_OBJS = module.o netdrv.o dev.o queue.o

CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)
DEST = /lib/modules/$(CURRENT)/kernel/$(MDIR)

obj-m := w5x00.o
w5x00-objs := $(DRIVER_OBJS)

default:
	$(MAKE) -C $(KDIR) M=$$PWD

install:
	sudo mkdir $(DEST)
	sudo cp $(TARGET) $(DEST)/$(TARGET)
	sudo /sbin/depmod -a

revert:
	@echo "Reverting to the original w5x00.ko."
	@mv -v $(DEST)/$(TARGET).orig $(DEST)/$(TARGET)

clean:
	rm -f *.o w5x00.ko .*.cmd .*.flags *.mod.c

-include $(KDIR)/Rules.make
