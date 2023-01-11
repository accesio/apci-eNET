#!/bin/bash

PHASE=1

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
led_flash $PHASE
((PHASE++))

echo 0 > /sys/block/mmcblk0boot0/force_ro
dd if=/opt/tiboot3.bin of=/dev/mmcblk0boot0 seek=0
dd if=/opt/tispl.bin of=/dev/mmcblk0boot0 seek=1024
dd if=/opt/u-boot.img of=/dev/mmcblk0boot0 seek=5120

echo DONE

echo WRITING PARTITION TABLE
led_flash $PHASE
((PHASE++))

sfdisk /dev/mmcblk0 << EOF
label: dos
label-id: 0x7a076551
device: /dev/mmcblk0
unit: sectors
sector-size: 512

/dev/mmcblk0p1 : start=        2048, size=    15267840, type=83
EOF

echo DONE

echo SETTING HW RESET ENABLE
led_flash $PHASE
((PHASE++))

mmc hwreset enable /dev/mmcblk0

echo DONE

echo SETTING EMMC PARTITION_CONFIG
led_flash $PHASE
((PHASE++))

mmc bootpart enable 1 1 /dev/mmcblk0
echo DONE

echo SETTING EMMC BOOT_BUS_CONDITIONS
led_flash $PHASE
((PHASE++))

mmc bootbus set single_backward x1 x8 /dev/mmcblk0

echo DONE

echo CREATING ext4 FILESYSTEM
led_flash $PHASE
((PHASE++))
mkfs.ext4 /dev/mmcblk0p1
echo DONE

echo WRITING ROOTFS
led_flash $PHASE
((PHASE++))

mount -t ext4 /dev/mmcblk0p1 /opt/mnt

tar -xf /opt/rootfs.tar -C /opt/mnt
umount /opt/mnt

echo DONE

led_set 1
