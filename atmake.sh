#!/bin/bash
export CROSS_COMPILE=/home/atudor/work/gcc-linaro-7.5.0-2019.12-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-
#export CROSS_COMPILE=/home/atudor/work/gcc-linaro-7.3.1-2018.05-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-

#make ARCH=arm sama5_defconfig &&
make ARCH=arm sama5d31_defconfig &&
make ARCH=arm menuconfig &&
make ARCH=arm -j 8 &&
make ARCH=arm dtbs
cp zImage /home/atudor/work/sam-ba_3.5/
