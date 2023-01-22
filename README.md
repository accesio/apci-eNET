# Building the eNET image
## overview
Building the eNET image involves first generating the kernel and bootloader with the TI sdk, then building the full image with buildroot.
## ti-sdk

### overview
Original work is based on [08.00.00.21 SDK](https://www.ti.com/tool/download/PROCESSOR-SDK-LINUX-AM64X/08.00.00.21)

## buildroot

### Download and run
ti-processor-sdk-linux-am64xx-evm-08.00.00.21-Linux-x86-Install.bin
The directory where you install it doesn't really matter, and is the ./sdk-root/
```sh
19:50:45:jdolan@work-laptop:~/eNET-image/ti-processor-sdk-linux-am64xx-evm-08.00.00.21$ls -l
total 702884
drwxr-xr-x  2 jdolan jdolan      4096 Aug 21 19:50 bin
drwxr-xr-x  9 jdolan jdolan      4096 Aug 11  2021 board-support
drwxr-xr-x 12 jdolan jdolan      4096 Aug 11  2021 docs
drwxr-xr-x  6 jdolan jdolan      4096 Aug 11  2021 example-applications
drwxr-xr-x  2 jdolan jdolan      4096 Aug 11  2021 filesystem
drwxrwxr-x  3 jdolan jdolan      4096 Aug 21 19:50 linux-devkit
-rwxr-xr-x  1 jdolan jdolan 719690148 Aug 11  2021 linux-devkit.sh
-rwxr-xr-x  1 jdolan jdolan     16528 Aug 10  2021 Makefile
-rwxr-xr-x  1 jdolan jdolan      1575 Aug 21 19:50 Rules.make
-rwxr-xr-x  1 jdolan jdolan      4188 Aug 10  2021 setup.sh
```

### Patch the Linux kernel
#### Needed to boot
This patch https://github.com/accesio/linux/commit/5c7ecc0b095a5c2be28efd8685755df3c11b669d needs to be applied to the kernel. If you are building with 08.00.00.21 you can just check out this commit directly

```sh
sdk-root/board-support/linux-5.10.41+gitAUTOINC+4c2eade9f7-g4c2eade9f7>git remote add aio git@github.com:accesio/linux.git
sdk-root/board-support/linux-5.10.41+gitAUTOINC+4c2eade9f7-g4c2eade9f7>git fetch aio
sdk-root/board-support/linux-5.10.41+gitAUTOINC+4c2eade9f7-g4c2eade9f7>git checkout 5c7ecc0b
```

#### importing driver to kernel source tree
The script import-to-kernel-tree.sh will setup a kernel source tree with the current driver source. It takes two parameters

1 (required) Path to the root of th linux source tree

2 (optional) Filename of the kernel config used during build.

Example:
```sh
17:01:33:jdolan@work-laptop:~/acces-git/eNET_TCP_Server/apci-eNET/image$./import-to-kernel-tree.sh /home/jdolan/eNET-image/ti-processor-sdk-linux-am64xx-evm-08.00.00.21/board-support/linux-5.10.41+gitAUTOINC+4c2eade9f7-g4c2eade9f7/ /home/jdolan/eNET-image/ti-processor-sdk-linux-am64xx-evm-08.00.00.21/board-support/linux-5.10.41+gitAUTOINC+4c2eade9f7-g4c2eade9f7/arch/arm64/configs/tisdk_am64xx-evm_defconfig
```

### patch u-boot
```sh
git remote add aio git@github.com:accesio/u-boot.git
git fetch aio
git checkout aio-enet
```


### Run the SDK make
Takes about seventeen minutes on my dev machine
```sh
sdk-root>make
```


## buildroot
### overview
Original work is based on https://buildroot.org/downloads/buildroot-2022.05.tar.gz
Need to decompress buildroot, copy config files, setup the overlay and build


### Download and decompress buildroot you will be using.
The place it is decompressed to is ./buildroot/
```sh
20:43:14:jdolan@work-laptop:~/eNET-image/buildroot-2022.05$ls -l
total 928
drwxrwxr-x    2 jdolan jdolan   4096 Jun  6 13:14 arch
drwxrwxr-x   76 jdolan jdolan   4096 Jun  6 13:14 board
drwxrwxr-x   26 jdolan jdolan   4096 Jun  6 13:14 boot
-rw-rw-r--    1 jdolan jdolan 475682 Jun  6 13:14 CHANGES
-rw-rw-r--    1 jdolan jdolan  29551 Jun  6 13:14 Config.in
-rw-rw-r--    1 jdolan jdolan 148773 Jun  6 13:14 Config.in.legacy
drwxrwxr-x    2 jdolan jdolan  16384 Jun  6 13:14 configs
-rw-rw-r--    1 jdolan jdolan  18767 Jun  6 13:14 COPYING
-rw-rw-r--    1 jdolan jdolan  75866 Jun  6 13:14 DEVELOPERS
drwxr-xr-x    5 jdolan jdolan   4096 Jun  6 13:21 docs
drwxrwxr-x   20 jdolan jdolan   4096 Jun  6 13:14 fs
drwxrwxr-x    2 jdolan jdolan   4096 Jun  6 13:14 linux
-rw-rw-r--    1 jdolan jdolan  46317 Jun  6 13:14 Makefile
-rw-rw-r--    1 jdolan jdolan   2292 Jun  6 13:14 Makefile.legacy
drwxrwxr-x 2602 jdolan jdolan  69632 Jun  6 13:14 package
-rw-rw-r--    1 jdolan jdolan   1075 Jun  6 13:14 README
drwxrwxr-x   13 jdolan jdolan   4096 Jun  6 13:14 support
drwxrwxr-x    3 jdolan jdolan   4096 Jun  6 13:14 system
drwxrwxr-x    5 jdolan jdolan   4096 Jun  6 13:14 toolchain
drwxrwxr-x    3 jdolan jdolan   4096 Jun  6 13:14 utils
```

### copy config
```
Copy image/eNET-buildroot-2022.05.config to buildroot/.config
mkdir buildroot/users
Copy image/acces-users.txt to buildroot/users/
```

### Setup the overlay
Notes:
* You do NOT use sudo in this case
* The mv after the make is because of the way buildroot merges the lib directories.

```sh
cd buildroot/
mkdir overlay
cd sdk-root/
DESTDIR=buildroot/overlay make install
cd buildroot/overlay
mv lib usr
mkdir -p etc/systemd/system
mkdir -p etc/modules-load.d
mkdir -p opt/aioenet/config.current
mkdir -p etc/sudoers.d
```
```
Copy image/eNET-AIO-TCPServer.service to buildroot/overlay/etc/systemd/system/
Copy image/apci.conf to buildroot/overlay/etc/modules-load.d
Copy image/DAC_Range.conf opt/aioenet/DAC_RangeCh0.conf
Copy image/DAC_Range.conf opt/aioenet/DAC_RangeCh1.conf
Copy image/DAC_Range.conf opt/aioenet/DAC_RangeCh2.conf
Copy image/DAC_Range.conf opt/aioenet/DAC_RangeCh3.conf
Copy the aionetd from the eNET_TCP_SERVER repo to buildroot/opt/
Copy image/sudoers-acces etc/sudoers.d/acces
```


### Run make in ./buildroot/
Takes about two hours on my machine. A lot of that is download, and it is building everything from source.
```
cd buildroot
make
```

## Creating the SD card
The SD card needs to have two partitions.
* Boot partition formatted as FAT. (Mine is 79MB)
* Rootfs partition formatted as Ext4. (You can make this take up the rest of the card)

## Copy files to boot partition
Must be owned by root so use sudo
```sh
sudo cp sdk-root/board-support/u-boot_build/a53/u-boot.img /path/to/boot/
sudo cp sdk-root/board-support/u-boot_build/a53/tispl.bin /path/to/boot/
sudo cp sdk-root/board-support/prebuilt-images/tiboot3.bin /path/to/boot/
```

## Extract files to rootfs partition
Must be owned by root so use sudo
```
sudo tar -xvf buildroot/output/images/rootfs.tar -C /path/to/rootfs
```


# enabling emmc boot

## copy needed files to sd card. Then boot from sd card
## copy u-boot files to boot partition
## fdisk /dev/mmcblk0p1
```
/dev/mmcblk1p1       2048 15269887 15267840  7.3G 83 Linux
```
## extract rootfs



# Building the docker (updated 2023 01 22)
## Overview
* The docker becomes a custom cross compiler for the image generated by the ti-sdk and buildroot.
* It requires two toolchains.
  * One is for the kernel
  * One is for userspace.
* Generating a new docker should only need to be done when
  * Making changes to the buildroot that add new libraries
  * Making changes to the kernel that break module compatibility

### Setting up file system. Need to copy files to a staging directory for the docker build
* When you are done the tree should look like this
```sh
./eNET-builder$tree -L 3
.
├── Dockerfile
└── opt
    ├── aarch64-buildroot-linux-gnu_sdk-buildroot
    │   ├── aarch64-buildroot-linux-gnu
    │   ├── bin
    │   ├── doc
    │   ├── etc
    │   ├── include
    │   ├── lib
    │   ├── lib64 -> lib
    │   ├── libexec
    │   ├── relocate-sdk.sh
    │   ├── sbin
    │   ├── share
    │   ├── usr -> .
    │   └── var
    ├── src
    │   └── linux
    └── ti-sdk-sysroot
        ├── environment-setup.d
        ├── etc
        ├── lib
        ├── post-relocate-setup.d
        ├── sbin
        ├── usr
        └── var

24 directories, 2 files
```

### kernel toolchain
* Copy the contents of ti-sdk-root/linux-devkit/sysroots/x86_64-arago-linux/usr/bin to Docker/opt/ti-sdk-sysroot
  * cp -r ti-sdk-root/linux-devkit/sysroots/x86_64-arago-linux/* eNET-builder/opt/ti-sdk-sysroot/
```sh
doppel@26887bead8ad:/opt/ti-sdk-sysroot$ ls -l
total 28
drwxr-xr-x  2 root root 4096 Aug 17 13:00 environment-setup.d
drwxr-xr-x  7 root root 4096 Aug 17 13:00 etc
drwxr-xr-x  2 root root 4096 Aug 17 13:00 lib
drwxr-xr-x  2 root root 4096 Aug 17 13:00 post-relocate-setup.d
drwxr-xr-x  2 root root 4096 Aug 17 13:00 sbin
drwxr-xr-x 10 root root 4096 Aug 17 13:00 usr
drwxr-xr-x  5 root root 4096 Aug 17 13:00 var
```

### kernel source
* copy the overlay/usr/lib/modules/5.10.41-g4c2eade9f7/ to eNET-builder/opt/src/linux/
  * rsync -r --copy-links /home/jdolan/eNET-image/buildroot-2022.05/overlay/usr/lib/modules/5.10.41-g4c2eade9f7/ linux/
```sh
./eNET-builder/opt/src/linux$ls -l
total 1276
drwxr-xr-x 31 jdolan jdolan   4096 Jan 22 10:53 build
drwxrwxr-x  2 jdolan jdolan   4096 Jan 22 10:53 extra
drwxrwxr-x 10 jdolan jdolan   4096 Jan 22 10:53 kernel
-rw-r--r--  1 jdolan jdolan 315654 Jan 22 10:53 modules.alias
-rw-r--r--  1 jdolan jdolan 320044 Jan 22 10:53 modules.alias.bin
-rw-rw-r--  1 jdolan jdolan  15992 Jan 22 10:53 modules.builtin
-rw-r--r--  1 jdolan jdolan  35713 Jan 22 10:53 modules.builtin.alias.bin
-rw-r--r--  1 jdolan jdolan  19038 Jan 22 10:53 modules.builtin.bin
-rw-rw-r--  1 jdolan jdolan 103994 Jan 22 10:53 modules.builtin.modinfo
-rw-r--r--  1 jdolan jdolan  52681 Jan 22 10:53 modules.dep
-rw-r--r--  1 jdolan jdolan  85009 Jan 22 10:53 modules.dep.bin
-rw-r--r--  1 jdolan jdolan    168 Jan 22 10:53 modules.devname
-rw-rw-r--  1 jdolan jdolan  26323 Jan 22 10:53 modules.order
-rw-r--r--  1 jdolan jdolan    484 Jan 22 10:53 modules.softdep
-rw-r--r--  1 jdolan jdolan 125517 Jan 22 10:53 modules.symbols
-rw-r--r--  1 jdolan jdolan 159953 Jan 22 10:53 modules.symbols.bin
drwxr-xr-x 31 jdolan jdolan   4096 Jan 22 10:53 source
```


### application toolchain
* The application toolchain can be generated by running `make sdk` in buildroot
* tar -xf buildroot/output/images/aarch64-buildroot-linux-gnu_sdk-buildroot.tar.gz -C eNET-builder/opt

### Contents of Dockerfile for eNET builder (You can ignore "file: could not find any valid magic files! (No such file or directory)" errors from relocate-sdk.sh
* To create the docker locally run `docker build -t enet-builder`
* To run the docker after it is created `docker run -v$(pwd):/home/doppel --rm -it --entrypoint bash enet-builder`
  * This is done in the root of your souce tree.
```Dockerfile

#If you see the following errors during the relocate-sdk.sh then you need to pick a "from" that has a compatible
#glibc version to the machine used to build the ti-sdk and buildroot. Nothing involving the cross-compile will work
#file: /lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.34' not found (required by file)
#file: /lib/x86_64-linux-gnu/libc.so.6: version `GLIBC_2.33' not found (required by /opt/aarch64-buildroot-linux-gnu_sdk-buildroot/bin/../lib/libmagic.so.1)
from ubuntu:22.04

RUN apt-get update && apt-get install build-essential -y
ENV PATH "$PATH:/opt/aarch64-buildroot-linux-gnu_sdk-buildroot/bin:/opt/ti-sdk-sysroot/usr/bin:"
ENV CC "aarch64-linux-gcc"
ENV GCC "aarch64-linux-gcc"
ENV KDIR "/opt/src/linux/build"
ENV CFLAGS "--sysroot=/opt/aarch64-buildroot-linux-gnu_sdk-buildroot/aarch64-buildroot-linux-gnu/sysroot"
COPY . /
WORKDIR /opt/aarch64-buildroot-linux-gnu_sdk-buildroot/
RUN ./relocate-sdk.sh
RUN useradd -ms /bin/bash doppel
USER doppel
WORKDIR /home/doppel
```


# Using the enet-builder docker

### There is a preexisting ACCES docker available on docker hub
```sh
docker pull jdolanacces/enet-builder
```
### Building kernel modules
* The KDIR and CC environment variables were already set in the environment when the docker was created
* Makefile will need to include the CROSS_COMPILE and ARCH options
```Makefile
obj-m += apci.o
CC		?= gcc
KDIR            ?= /lib/modules/$(shell uname -r)/build

apci-objs :=      \
    apci_fops.o   \
	apci_dev.o

all:
	$(MAKE) CC=$(CC) -C $(KDIR) M=$(CURDIR) ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules

clean:
	$(MAKE) CC=$(CC) -C $(KDIR) M=$(CURDIR) ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- clean

install:
	$(MAKE) CC=$(CC) -C $(KDIR) M=$(CURDIR) ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules_install
	depmod -A
	modprobe -r apci
	modprobe apci
```
#### Example run
```sh
13:28:45:jdolan@dalaptop:~/acces-git/eNET_TCP_Server/apci-eNET$docker run -v$(pwd):/home/doppel --rm -it --entrypoint bash enet-builder
doppel@4519aac21004:~$ ls
Makefile   README.txt	  apci_dev.c  apci_fops.c  apci_ioctl.h  build-and-copy.sh  eNET-AIO.h	     image
README.md  apci_common.h  apci_dev.h  apci_fops.h  apcilib	 build.sh	    eNET-builder.sh
doppel@4519aac21004:~$ make
make CC=aarch64-linux-gcc -C /opt/src/linux/build M=/home/doppel ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
make[1]: Entering directory '/opt/src/linux/build'
  CC [M]  /home/doppel/apci_fops.o
  CC [M]  /home/doppel/apci_dev.o
  LD [M]  /home/doppel/apci.o
  MODPOST /home/doppel/Module.symvers
  CC [M]  /home/doppel/apci.mod.o
  LD [M]  /home/doppel/apci.ko
make[1]: Leaving directory '/opt/src/linux/build'
doppel@4519aac21004:~$ exit
exit
13:28:58:jdolan@dalaptop:~/acces-git/eNET_TCP_Server/apci-eNET$file apci.ko
apci.ko: ELF 64-bit LSB relocatable, ARM aarch64, version 1 (SYSV), BuildID[sha1]=074d3f72c5d26e5a78a6c52ef41c31ad59011b12, not stripped
13:29:01:jdolan@dalaptop:~/acces-git/eNET_TCP_Server/apci-eNET$
```

### Building userspace
* CFLAGS is already set in the environment when building the docker.
  * This is to point the toolchain at all the libraries available on the eNET
  * If you need to modify CFLAGS be sure to append to them and not overwrite.
* CC and GCC are already set in the environment.
  * For GCC override with g++ to use standard libraries without including them explicitly

```Makefile

all: aioenetd

#GCC doesn't include libstdc++, so just override it here
GCC := aarch64-linux-g++

test:	test.cpp Makefile eNET-types.h TMessage.cpp TMessage.h TError.h TError.cpp apcilib.cpp logging.h adc.cpp adc.h
	$(GCC) -g -Wfatal-errors -std=gnu++2a -o test test.cpp TError.cpp logging.cpp TMessage.cpp apcilib.cpp adc.cpp -lm -lpthread -latomic -O3



aioenetd:	Makefile $(wildcard *.h) $(wildcard *.cpp) $(wildcard DataItems/*.cpp) $(wildcard DataItems/*.h)
	$(GCC) -g -Wfatal-errors -std=gnu++2a -o aioenetd $(wildcard *.cpp) $(wildcard DataItems/*.cpp) -lm -lpthread -latomic -O3

clean:
	rm -f test aioenetd
```

#### example run
```sh
13:36:50:jdolan@dalaptop:~/acces-git/eNET_TCP_Server$docker run -v$(pwd):/home/doppel --rm -it --entrypoint bash enet-builder
doppel@f6bd89e0d3f5:~$ ls
DataItems   TError.h	  adc.h		apci.h	      config.cpp	logging.cpp  safe_queue.h
Makefile    TMessage.cpp  aioenetd.cpp	apci_ioctl.h  config.h		logging.h    test.c_p
README.md   TMessage.h	  apci-eNET	apcilib.cpp   eNET-AIO16-16F.h	my-reboot
TError.cpp  adc.cpp	  apci.cpp	apcilib.h     eNET-types.h	reboot.c
doppel@f6bd89e0d3f5:~$ make
aarch64-linux-g++ -g -Wfatal-errors -std=gnu++2a -o aioenetd TError.cpp TMessage.cpp adc.cpp aioenetd.cpp apci.cpp apcilib.cpp config.cpp logging.cpp DataItems/ADC_.cpp DataItems/BRD_.cpp DataItems/CFG_.cpp DataItems/DAC_.cpp DataItems/REG_.cpp DataItems/TDataItem.cpp -lm -lpthread -latomic -O3
doppel@f6bd89e0d3f5:~$ exit
exit
13:37:36:jdolan@dalaptop:~/acces-git/eNET_TCP_Server$file aioenetd
aioenetd: ELF 64-bit LSB pie executable, ARM aarch64, version 1 (GNU/Linux), dynamically linked, interpreter /lib/ld-linux-aarch64.so.1, for GNU/Linux 3.7.0, with debug_info, not stripped
13:37:39:jdolan@dalaptop:~/acces-git/eNET_TCP_Server$
```
