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
This patch https://github.com/accesio/linux/commit/5c7ecc0b095a5c2be28efd8685755df3c11b669d needs to be applied to the kernel. If you are building with 08.00.00.21 you can just check out this commit directly

There is another commit that shows how to integrate the APCI driver into the build. https://github.com/accesio/linux/commit/87c364423b046209a630e935e627da50db8608a7
```sh
sdk-root/board-support/linux-5.10.41+gitAUTOINC+4c2eade9f7-g4c2eade9f7>git remote add aio git@github.com:accesio/linux.git
sdk-root/board-support/linux-5.10.41+gitAUTOINC+4c2eade9f7-g4c2eade9f7>git fetch aio
sdk-root/board-support/linux-5.10.41+gitAUTOINC+4c2eade9f7-g4c2eade9f7>git checkout 5c7ecc0b
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
mkdir opt

```
```
Copy image/eNET-AIO-TCPServer.service to buildroot/overlay/etc/systemd/system/
Copy image/apci.conf to buildroot/overlay/etc/modules-load.d
Copy the aionetd from the eNET_TCP_SERVER repo to buildroot/opt/
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



# Building the docker (not complete 2022 08 24)
## overview
The docker becomes a custom cross compiler for the image generated by the ti-sdk and buildroot. It requires two toolchains. One toolchain is for the kernel and the other is for userspace.

### Setting up files system. Need to copy files to a staging directory for the docker build
```sh
docker-root$ls -l
total 8
-rw-rw-r-- 1 jdolan jdolan  561 Aug 18 05:16 Dockerfile
drwxrwxr-x 6 jdolan jdolan 4096 Aug 17 05:59 opt
```

### kernel toolchain
```
sdk-root/linux-devkit/sysroots/x86_64-arago-linux/usr/bin
```

### application toolchain
```sh
make sdk
cd /docker-root/opt
tar -xvf buildroot/output/images/aarch64-buildroot-linux-gnu_sdk-buildroot.tar.gz
```

### Complete Dockerfile for eNET builder (You can ignore "file: could not find..." errors from relocate-sdk.sh
```Dockerfile
from ubuntu:18.04

RUN apt-get update && apt-get install build-essential -y
ENV PATH "$PATH:/opt/aarch64-buildroot-linux-gnu_sdk-buildroot/bin:/opt/ti-sdk-sysroot/usr/bin:"
ENV CC "aarch64-linux-gcc"
ENV GCC "aarch64-linux-gcc"
ENV KDIR "/opt/src/linux/build"
ENV CFLAGS "--sysroot=/opt/aarch64-buildroot-linux-gnu_sdk-buildroot/aarch64-buildroot-linux-gnu/sysroot"
COPY . /
WORKDIR /opt/aarch64-buildroot-linux-gnu_sdk-buildroot/
RUN ./relocate-sdk.sh
#RUN make -C /opt/src/linux/build prepare
RUN useradd -ms /bin/bash doppel
USER doppel
WORKDIR /home/doppel
```

