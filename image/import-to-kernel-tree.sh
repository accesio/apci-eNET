#!/bin/bash

function usage {
  printf "Script is intended to be run from the image directory of the apci-eNET\n"
  printf "repository with one or two arguments\n"
  printf "1(required): Path to the root of the kernel source tree to be modified.\n"
  printf "2(optional): Full path of config file do be modified\n"
}

# Creates a string that is the kconfig located at linux-root/drivers/Kconfig
# modified to include the source statement for the ACCES driver
function modify_drivers_kconfig {
while read p; do
  if [[ "$p" == *"endmenu"* ]]; then
    printf "\nsource \"drivers/acces/Kconfig\"\n\n"
  fi
  printf "$p\n"
done < Kconfig
}

# Creates a string that is a kernel config with the ACCES_ENET driver set to build
# as a module
# $1 file that is kernel config to modify
function modify_kernel_config {
while read p; do
  if [[ "$p" == *"end of Device Drivers"* ]]; then
    printf "CONFIG_ACCES_ENET_MODULE=m\n"
  fi
  printf "$p\n"
done <$1
}



####main

if [[ "$#" -ne 1 && "$#" -ne 2 ]] ; then
  usage
  exit 1
fi

#push puts us in linux root
pushd $1
cd drivers

NEW_DRIVERS_KCONFIG=$(cat<<EOF
$(modify_drivers_kconfig)
EOF
)
printf "$NEW_DRIVERS_KCONFIG\n" > Kconfig

cat << 'EOF' >> Makefile
obj-$(CONFIG_ACCES_ENET_MODULE)		+= acces/
EOF

mkdir acces
cd acces

cat << EOF > Kconfig
config ACCES_ENET_MODULE
        tristate "ACCES eNET PCI driver"
        help
                Select this to add ACCES eNET driver
EOF

cat << 'EOF' > Makefile
obj-$(CONFIG_ACCES_ENET_MODULE)		+= apci.o

apci-objs :=      \
		apci_fops.o   \
	apci_dev.o
EOF

DRIVER_DEST_DIR=$PWD
#This should put us back in apci-eNET/image
popd

cp ../apci_common.h $DRIVER_DEST_DIR
cp ../apci_dev.c $DRIVER_DEST_DIR
cp ../apci_dev.h $DRIVER_DEST_DIR
cp ../apci_fops.c $DRIVER_DEST_DIR
cp ../apci_fops.h $DRIVER_DEST_DIR
cp ../apci_ioctl.h $DRIVER_DEST_DIR
cp ../eNET-AIO.h $DRIVER_DEST_DIR

echo Driver placed in kernel tree

if [ "$#" -eq 2 ]; then
echo $2
echo "Going to modify kernel config"

NEW_KERNEL_CONFIG=$(cat<<EOF
$(modify_kernel_config $2)
EOF
)
printf "$NEW_KERNEL_CONFIG\n" > $2

fi


