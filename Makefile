obj-m += apci.o
CC		?= gcc
KDIR            ?= /lib/modules/$(shell uname -r)/build

apci-objs :=      \
    apci_fops.o   \
	apci_dev.o

INSTALL_MOD_PATH := /lib/modules/$(shell uname -r)/

all:
	$(MAKE) CC=$(CC) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) CC=$(CC) -C $(KDIR) M=$(CURDIR) clean

install:
	$(MAKE) CC=$(CC) -C $(KDIR) M=$(CURDIR) INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) modules_install
	depmod -A
	modprobe -r apci
	modprobe apci
