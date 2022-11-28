#!/bin/bash

led_flash () {
	echo $1
	for (( i = 1; i<=$1; i++ ))
       	do
		echo 1 > /sys/devices/platform/leds/leds/cpu-0\ activity/brightness
		sleep 0.1
		echo 0 > /sys/devices/platform/leds/leds/cpu-0\ activity/brightness
		sleep 0.1
	done
}

led_set () {

	echo $1 > /sys/devices/platform/leds/leds/cpu-0\ activity/brightness
}


echo FLASHING BOOTLOADER

led_flash 1

echo 0 > /sys/block/mmcblk0boot0/force_ro
dd if=/opt/tiboot3.bin of=/dev/mmcblk0boot0 seek=0
dd if=/opt/tispl.bin of=/dev/mmcblk0boot0 seek=1024
dd if=/opt/u-boot.img of=/dev/mmcblk0boot0 seek=5120

echo DONE

echo WRITING PARTITION TABLE

led_flash 2

sfdisk /dev/mmcblk0 << EOF
label: dos
label-id: 0x7a076551
device: /dev/mmcblk0
unit: sectors
sector-size: 512

/dev/mmcblk0p1 : start=        2048, size=    15267840, type=83
EOF

echo DONE

echo WRITING ROOTFS

led_flash 3

mount -t ext4 /dev/mmcblk0p1 /opt/mnt

tar -xf /opt/rootfs.tar -C /opt/mnt
umount /opt/mnt

echo DONE

led_set 1
