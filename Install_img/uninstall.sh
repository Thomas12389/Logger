#! /bin/bash

# 检查权限
if (( $(id -u) != 0 )); then
	echo "Permission denied"
    exit 0
fi

# 检查是否为 Owasys 的设备
if [ ! -f "/etc/issue.owa" ]; then
	echo "The device is not made by Owasys"
	exit 4
else
	fw_version=$(cat < /etc/issue.owa | awk -F" " '{print $5}')
	# 打印设备相关信息
	echo "Device Type: $(cat < /etc/issue.owa | awk -F" " '{print $3}')"
	echo "The SN: $(uname -n)"
	echo "FW Version： ${fw_version}"
	printf "%0.s*" {1..50}
	echo 
fi

# 旧文件处理脚本
old_files_deal_sh=/opt/TFM_scripts/deal_old_files.sh

# 获取可执行文件实际路径
exe_real_path=$(readlink /usr/bin/WINDHILL_WCP)
exe_real_name=$(basename "${exe_real_path}")
# WLAN 配置脚本存放位置
WLAN_script_path=/opt/TFM_scripts/
# 获取安装基目录
exe_base_path=${exe_real_path:0:$(( ${#exe_real_path} - ${#exe_real_name} )) }

# 移除安装目录
function RemoveInstallFiles() {
	if [ -e "${exe_base_path}" ]; then
		echo "Path \"${exe_base_path}\" exists"
		rm -rf "${exe_base_path}"
	fi

	if [ -f "${old_files_deal_sh}" ]; then
		echo "File \"${old_files_deal_sh}\" exists"
		rm -rf ${old_files_deal_sh}
	fi
	
}

# 移除 WLAN 脚本
function RemoveWLANScript() {

	if [ -e "${WLAN_script_path}" ]; then
		rm -rf ${WLAN_script_path}/WLAN*
	fi
}


# 移除库文件
function RemoveLibFiles() {
    local lib_target_dir=/usr/lib
	local cur_dir
	local lib_files_list="libssh2.so.1.0.1 libcrypto.so.1.0.0 libssl.so.1.0.0 libmosquitto.so.1 libftp.so.4.0 libVector_BLF.so.2.4.1"

    cur_dir=$(pwd)

    cd "${lib_target_dir}" || return 1

    # 循环删除库
    for lib_file in ${lib_files_list}
	do
		if [ -f "${lib_file}" ]; then
			echo "find file ${lib_file}"
            rm -rf "${lib_file}"
		fi
		
	done
    # 删除 libssh2 的软连接
	if [ -h "libssh2.so.1" ]; then
		rm -rf libssh2.so.1
	fi
    # 删除 libftp 的软连接
	if [ -h "libftp.so.4" ]; then
		rm -rf libftp.so.4
	fi
	# 删除 libVector_BLF 的软连接
	if [ -h "libVector_BLF.so.2" ]; then
		rm -rf libVector_BLF.so.2
	fi

    cd "${cur_dir}" || return 1

	return 0
}

# 取消自动启动
function DisAutoRun() {

    local service_path=/etc/systemd/system
	local service_name=rc-local.service		#wh-wcp.service
	local service_rclocal=/etc/rc.local
	local service_script=/home/debian/TBox.sh
	local service_status

    service_status=$(systemctl status ${service_name} | grep "Active" | awk -F":" '{print $2}' | awk -F" " '{print $1}')
    if [[ -n "${service_status}" ]]; then
        echo "\"${service_name}\" status: \"${service_status}\""

        if [[ "${service_status}" == "active" ]]; then
            systemctl stop ${service_name}
	    fi

        systemctl disable ${service_name}
    fi

    # 删除自定义脚本文件
	if [ -f ${service_script} ]; then
		echo "service file \"${service_script} \" exists"
        rm -rf ${service_script}
	fi

    # 删除 rc.local 脚本
	if [ -f ${service_rclocal} ]; then
		echo "service file \"${service_rclocal} \" exists"
        rm -rf ${service_rclocal}
	fi

    # 删除服务文件
    if [ -f "${service_path}/${service_name}" ]; then
		echo "service file \"${service_path}/${service_name}\" exists"
        rm -rf "${service_path:?}/${service_name}"
	fi
}

function Stop() {
    # 停止程序，删除软连接
    for prog in WINDHILL_WCP
    do
        progID=$(pidof ${prog})
        if (( 0 == $? )); then
            echo "Stop ${prog}"
            kill -s SIGINT "${progID}"
        fi

        rm -f /usr/bin/${prog}
    done  
}

Stop

# DisAutoRun

RemoveLibFiles

RemoveWLANScript

RemoveInstallFiles

return 0
