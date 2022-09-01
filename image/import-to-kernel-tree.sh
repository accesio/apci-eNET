#!/bin/bash

function usage {
  echo "Script is intended to be run from the image directory of the apci-eNET repository with one argument which is the path to the root of the kernl source tree to be modified."
}

function modify_driver_kconfig {
NEW_KCONFIG=$(cat << EOF
this is the new KConfig
EOF
)
echo $NEW_KCONFIG
}

if [ "$#" -ne 1 ]; then
  usage
fi

pushd $1
cd drivers
modify_driver_kconfig
popd


