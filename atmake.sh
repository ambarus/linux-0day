#!/bin/bash
#export CROSS_COMPILE=/home/atudor/work/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
#export CROSS_COMPILE=/home/atudor/work/gcc-linaro-7.3.1-2018.05-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
export CROSS_COMPILE=arm-linux-gnueabi-

#make ARCH=arm COMPILED_SOURCE=1 cscope

#make ARCH=arm sama7_defconfig &&
#make ARCH=arm at91_dt_defconfig &&
make ARCH=arm sama5_defconfig &&
#make ARCH=arm sama5_defconfig
#make ARCH=arm menuconfig
#make ARCH=arm -j 16 C=1
make ARCH=arm -j 16 W=1 &&
make ARCH=arm dtbs

#sudo mount /dev/mmcblk0p2 /mnt
#sudo make INSTALL_MOD_PATH=/mnt modules_install -j8
#sync
#sudo umount /mnt

sudo cp arch/arm/boot/dts/microchip/at91-sama5d2_xplained.dtb /tftpboot
sudo cp arch/arm/boot/zImage /tftpboot
