#!/bin/bash

if [ $# != 1 ];then
    echo "Usage: $0 {gcc53|gcc83}"
    exit 1
fi

case $1 in
    gcc53)
        MY_CC=arm-linux-gnueabihf-gcc
        MY_CXX=arm-linux-gnueabihf-g++
        ;;
    gcc83)
        MY_CC=arm-linux-gnueabihf-8.3-gcc
        MY_CXX=arm-linux-gnueabihf-8.3-g++
        ;;
    *)
        echo "Usage: $0 {gcc53|gcc83}"	
        exit 1
        ;;
esac

MY_SHELLDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" || exit; pwd)
MY_DESTDIR=${MY_SHELLDIR}/arm_$1_build

rm -rf "${MY_DESTDIR:?}"

if [ "${MY_SHELLDIR}" != "$(pwd)" ];then
	cd "${MY_SHELLDIR}" || exit
fi

make CC=${MY_CC} CXX=${MY_CXX}
make install DESTDIR="${MY_DESTDIR}"
make clobber


