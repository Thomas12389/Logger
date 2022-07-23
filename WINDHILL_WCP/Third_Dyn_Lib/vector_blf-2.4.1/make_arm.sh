#!/bin/bash

if [ $# != 1 ];then
    echo "Usage: $0 {gcc53|gcc83}"
    exit 1
fi

case $1 in
    gcc53)
        cmake_plugin_file=OwaGcc53Compile.cmake
        ;;
    gcc83)
        cmake_plugin_file=OwaGcc83Compile.cmake
        ;;
    *)
        echo "Usage: $0 {gcc53|gcc83}"	
        exit 1
        ;;
esac

# 本脚本路径
this_file_path=$(cd "$(dirname "${BASH_SOURCE[0]}")" || exit; pwd)

build_dir="arm_""$1""_build"
install_dir="/arm_""$1""_install"

if [ -d "${build_dir}" ]; then
    rm -rf "${build_dir}"
fi

mkdir "${build_dir}"
cd "${build_dir}" || exit 2
cmake -DCMAKE_TOOLCHAIN_FILE=../"${cmake_plugin_file}" ..
make
make install DESTDIR=../"${install_dir}"
# 删除 build 目录
cd "${this_file_path}" || exit 2
rm -rf "${build_dir}"

exit 0