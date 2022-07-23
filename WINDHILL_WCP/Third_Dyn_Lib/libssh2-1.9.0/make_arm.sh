#!/bin/bash

if [ $# != 1 ];then
    echo "Usage: $0 {gcc53|gcc83}"
    exit 1
fi

case $1 in
    gcc53)
        MY_CC=arm-linux-gnueabihf-gcc
        MY_CXX=arm-linux-gnueabihf-g++
        LIB_PREFIX=/opt/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/arm-linux-gnueabihf/libc/usr/lib
        ;;
    gcc83)
        MY_CC=arm-linux-gnueabihf-8.3-gcc
        MY_CXX=arm-linux-gnueabihf-8.3-g++
        LIB_PREFIX=/opt/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/arm-linux-gnueabihf/libc/usr/lib
        ;;
    *)
        echo "Usage: $0 {gcc53|gcc83}"	
        exit 1
        ;;
esac

MY_SHELLDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" || exit; pwd)
MY_DESTDIR=${MY_SHELLDIR}/arm_$1_build

rm -rf "${MY_DESTDIR:?}/*"

if [ "${MY_SHELLDIR}" != "$(pwd)" ];then
	cd "${MY_SHELLDIR}" || exit
fi



#make distclean

"${MY_SHELLDIR}"/configure CC=${MY_CC} CXX=${MY_CXX} --host=arm-linux-gnueabihf --prefix="${MY_DESTDIR}" --with-libssl-prefix=${LIB_PREFIX} --with-libz-prefix=${LIB_PREFIX}

make install-exec -j "$(grep -c "cpu cores" /proc/cpuinfo)"
make clean
