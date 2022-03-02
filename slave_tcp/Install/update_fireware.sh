#! /bin/bash

:<<!
return value:
	0		成功
	2		解压错误
	3		卸载错误
	4		安装错误
!

# 检查权限
if (( $(id -u) != 0 )); then
	echo "Permission denied"
    exit 255
fi

# 判断 WINDHILL_WCP 是否在运行
pidof WINDHILL_WCP
if (( 0 == $? )); then
    exit 1
fi


# 当前路径
work_dir=$(pwd)
# 更新包绝对路径
fireware_path=/opt/DL_Fireware
# 更新包名称
fireware_name=Fireware.img
# 存放更新包临时文件夹
fireware_temp_dir=/opt/DL_Fireware/temp
# 卸载脚本
fireware_uninstall_script="${fireware_temp_dir}/uninstall.sh"
# 安装脚本
fireware_install_script="${fireware_temp_dir}/install.sh"

if [ ! -d "${fireware_temp_dir}" ]; then
	echo "directory \"${fireware_temp_dir}\" not exists, create it"
	mkdir ${fireware_temp_dir} || exit 2
fi

cd ${fireware_path} || exit 2

tar -zxvf ${fireware_name} -C ${fireware_temp_dir} || exit 2

cd ${fireware_temp_dir} || exit 2

# 执行卸载脚本
source ./uninstall.sh || exit 3

# 执行安装脚本
source ./install.sh || exit 4

cd "${work_dir}" || exit 0
rm -rf ${fireware_temp_dir}

# 正常更新完成
exit 0
