obj-m += apci.o
CC		?= gcc
KDIR?=/home/jhentges/dev/linux-enet/linux
PWD := $(shell pwd)

apci-objs :=      \
    apci_fops.o   \
	apci_dev.o


all:
	$(MAKE) -C "$(KDIR)" M="$(PWD)" ARCH=arm64 CROSS_COMPILE=aarch64-oe-linux- CC=aarch64-oe-linux-gcc modules

clean:
	$(MAKE) -C "$(KDIR)" M="$(PWD)" clean

install:
	$(MAKE) CC=$(CC) -C $(KDIR) M=$(CURDIR) ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules_install
	depmod -A
	modprobe -r apci
	modprobe apci

