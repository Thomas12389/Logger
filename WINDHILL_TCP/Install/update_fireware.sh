#! /bin/bash

:<<!
暂未使用
参数:
	$1		更新包路径
	$2      更新包名称
    $3      更新包解压后临时目录
!


:<<!
return value:
	0		成功
	1		解压错误
	2		卸载错误
	3		安装错误
	4		wcp 正在运行
	5		wcp 与 tcp 版本部匹配
    6       获取欲更新 wcp 版本号错误
    7       获取 tcp 版本号错误
!

# Usage: versionMatch "1.2.3" "1.1.7"
# 返回 0 表示匹配
function versionMatch () {
    function subVersion () {
        echo -e "${1%%"."*}"
    }

    local v1
    v1=$(echo -e "${1}" | tr -d '[:space:]')
    local v2
    v2=$(echo -e "${2}" | tr -d '[:space:]')
    local v1Sub
    v1Sub=$(subVersion "$v1")
    local v2Sub
    v2Sub=$(subVersion "$v2")
    
    if (( v1Sub != v2Sub )); then
        return 1
    fi

    return 0
}

# 检查权限
if (( $(id -u) != 0 )); then
	echo "Permission denied"
    exit 255
fi

# 判断 WINDHILL_WCP 是否在运行
if pidof WINDHILL_WCP; then
# if (( 0 == $? )); then
    exit 4
fi


# 当前路径
work_path=$(pwd)
# 本脚本路径
this_file_path=$(cd "$(dirname "${BASH_SOURCE[0]}")" || exit; pwd)
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
	mkdir ${fireware_temp_dir} || exit 1
fi

cd ${fireware_path} || exit 1

tar -zxvf ${fireware_name} -C ${fireware_temp_dir} || exit 1

cd ${fireware_temp_dir} || exit 1

# 版本号匹配
new_wcp_version=$(awk -F '=' '/\[info\]/{a=1}a==1&&$1~/version/{print $2; exit}' info.ini) || exit 6
tcp_version=$(awk -F '=' '/\[info\]/{a=1}a==1&&$1~/version/{print $2; exit}' "${this_file_path}""/info.ini") || exit 7

versionMatch "${new_wcp_version}" "${tcp_version}" || exit 5

# 执行卸载脚本
source ./uninstall.sh || exit 2

# 执行安装脚本
source ./install.sh || exit 3

cd "${work_path}" || exit 0
rm -rf ${fireware_temp_dir}
# rm -rf "${fireware_path}""/*"

# 正常更新完成
exit 0
