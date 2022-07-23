# 设置交叉编译
set(CMAKE_SYSTEM_NAME Linux)

# 设置交叉编译器
set(TOOLCHAIN_PATH  /opt/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PATH}/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PATH}/bin/arm-linux-gnueabihf-g++)

set(CMAKE_CXX_FLAGS "-std=c++11 -Os")

# 设置交叉编译对应的一些路径
set(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_PATH}/arm-linux-gnueabihf/include/c++/5.3.1/
                        ${TOOLCHAIN_PATH}/arm-linux-gnueabihf/libc/usr/include/
                        ${TOOLCHAIN_PATH}/arm-linux-gnueabihf/libc/usr/lib/
                        
 )

set(ZLIB_INCLUDE_DIR ${TOOLCHAIN_PATH}/arm-linux-gnueabihf/libc/usr/include)
set(ZLIB_LIBRARY ${TOOLCHAIN_PATH}/arm-linux-gnueabihf/libc/usr/lib/libz.so)

# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

