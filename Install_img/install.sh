#! /bin/bash

:<<!
return value:
	0		成功
	1		命令执行错误
	2		目录不存在
	3		文件不存在
	4		不是 Owasys 的设备
	5		服务启动失败
	6		程序正在运行
			其他错误
!

# 检查权限
if (( $(id -u) != 0 )); then
	echo "Permission denied"
    exit 0
fi

# 安装脚本路径
this_file_path=$(cd "$(dirname "${BASH_SOURCE[0]}")" || exit; pwd)
# WLAN 配置脚本存放位置，和程序密切相关，不要随意更改
TFM_script_path=/opt/TFM_scripts
# 旧文件处理脚本
old_files_deal_source_sh=${this_file_path}/files/deal_old_files.sh
old_files_deal_target_sh=${TFM_script_path}/deal_old_files.sh
# 公共文件路径
common_file_path=${this_file_path}/files/common
# 默认安装路径
default_install_path=/opt/DL_Logger
wcp_install_path=${default_install_path}
# 版本信息
version_ini_path=${this_file_path}
version_ini_name=info.ini

# 可执行程序软连接
softlink_wcp=/usr/bin/WINDHILL_WCP
softlink_tcp=/usr/bin/WINDHILL_TCP
# 固件版本号
declare fw_version
# 工具链版本
#gcc_version=0.0.0
install_file_dir=gcc00


# 获取工具链版本
function GetToolVersion() {

	local FWVersion_MAJ

	FWVersion_MAJ=$(echo "${fw_version}" | awk -F"." '{print $1}')

	case ${FWVersion_MAJ} in
    	V2)
    		#gcc_version=8.3.0
			install_file_dir=${this_file_path}/files/gcc83
        	;;
    	V1)
			#gcc_version=5.3.0
			install_file_dir=${this_file_path}/files/gcc53
        	;;
    	*)
        	#gcc_version=5.3.0
			install_file_dir=${this_file_path}/files/gcc53
        	;;
	esac
}

function CopyWLANScript() {

	local cur_dir
	local WLANshList="WLAN_start.sh WLAN_stop.sh"

	cur_dir=$(pwd)
	# 启动 WiFi/BT 模块
	if Start_BT_WiFi 1; then
		# 等待足够的时间，让 mlan0 启动(如果存在)
		sleep 10s
	
    	if ip link set mlan0 up; then
			# 生成 WLAN 配置的脚本拷贝到指定位置
			if [ ! -e "${TFM_script_path}" ]; then
				mkdir -p ${TFM_script_path}
			fi
			
			cp "${this_file_path}"/files/WLAN/*.sh ${TFM_script_path}

			cd "${TFM_script_path}" || return 1
			for sh in ${WLANshList}
			do
				chmod a+x "${sh}"
			done
			cd "${cur_dir}" || return 1
		fi

		Start_BT_WiFi 0
	fi

	return 0
}

# 拷贝库文件
function CopyLibFiles() {

	local lib_target_dir=/usr/lib
	local lib_source_dir
	local cur_dir
	local lib_files_list="libssh2.so.1.0.1 libcrypto.so.1.0.0 libssl.so.1.0.0 libmosquitto.so.1 libftp.so.4.0 libVector_BLF.so.2.4.1"

	cur_dir=$(pwd)
	# TODO:目录结构可能会调整
	lib_source_dir=${install_file_dir}/lib

	if [ ! -d "${lib_source_dir}" ]; then
		echo "directory \"${lib_source_dir}\" not exists"
		return 2
	fi
	
	cd "${lib_source_dir}" || return 1
	# 检查所需要的库文件是否都存在
	for lib_file in ${lib_files_list}
	do
		if [ ! -f "${lib_file}" ]; then
			echo "lib file \"${lib_file}\" not exists."
			return 3
		else
			echo "find file ${lib_file}"
		fi
		
	done

	for lib_file in ${lib_files_list}
	do
		# 目标文件夹存在完全相同的文件
		if cmp -s "${lib_file}" "${lib_target_dir}/${lib_file}"; then
			echo "\"${lib_source_dir}/${lib_file}\" is same as \"${lib_target_dir}/${lib_file}\""
			continue
		fi
		
		cp "${lib_file}" ${lib_target_dir} || return 1
	done

	cd "${lib_target_dir}" || return 1
	# 创建 libssh2 的软连接
	if [ ! -h "libssh2.so.1" ]; then
		ln -s libssh2.so.1.0.1 libssh2.so.1 || return 1
	fi
	# 创建 libftp 的软连接
	if [ ! -h "libftp.so.4" ]; then
		ln -s libftp.so.4.0 libftp.so.4 || return 1
	fi
	# 创建 libVector_BLF 的软连接
	if [ ! -h "libVector_BLF.so.2" ]; then
		ln -s libVector_BLF.so.2.4.1 libVector_BLF.so.2 || return 1
	fi

	cd "${cur_dir}" || return 1

	return 0
}

# 安装 DL_Logger 前的准备
function InstallLoggerPro() {

	# 检查是否为 Owasys 的设备
	if [ ! -f "/etc/issue.owa" ]; then
		echo "The device is not made by Owasys"
		return 4
	else
		fw_version=$(cat < /etc/issue.owa | awk -F" " '{print $5}')
		# 打印设备相关信息
		echo "Device Type: $(cat < /etc/issue.owa | awk -F" " '{print $3}')"
		echo "The SN: $(uname -n)"
		echo "FW Version： ${fw_version}"
		printf "%0.s*" {1..50}
		echo 
	fi

	GetToolVersion

    # 安装目录不存在
    if [[ ! -d ${wcp_install_path} ]]; then
        printf "mkdir install path: %s\n" "${wcp_install_path}"
        mkdir -p "${wcp_install_path}"
    fi

	return 0
}

# 安装
function InstallLogger() {

	local pro_path
	local pro_wcp_exe
	local is_pro_run
	# TODO：路径可能会更改
	pro_path=${install_file_dir}
	pro_wcp_exe=${pro_path}/WINDHILL_WCP

	is_pro_run=$(ps aux | grep -v "grep" | grep -c "WINDHILL_WCP")
	if (( is_pro_run != 0 )); then
		echo "WINDHILL_WCP is running."
		return 6
	fi

	# wcp 采集程序
	cp "${pro_wcp_exe}" ${wcp_install_path} || return 1

	local exe_real_path="${wcp_install_path}"/WINDHILL_WCP
	chmod a+x ${exe_real_path}

	# 建立软连接
	if [ ! -h "${softlink_wcp}" ]; then
		ln -s ${exe_real_path} ${softlink_wcp}
	fi

	# 拷贝旧文件处理脚本
	if [ ! -e "${TFM_script_path}" ]; then
		mkdir -p ${TFM_script_path}
	fi
	cp "${old_files_deal_source_sh}" "${old_files_deal_target_sh}" || return $?
	ret=$?
	if (( ret != 0 )); then
		return ${ret}
	fi
	
	chmod a+x ${old_files_deal_target_sh}

	CopyLibFiles
	ret=$?
	if (( ret != 0 )); then
		return ${ret}
	fi

	CopyWLANScript
	ret=$?
	if (( ret != 0 )); then
		return ${ret}
	fi

	# 复制版本信息
	cp "${version_ini_path}/${version_ini_name}" "${wcp_install_path}" || return 1
	chmod +r "${wcp_install_path}/${version_ini_name}" || return 1
	
	return 0
}

InstallLoggerPro
retval=$?
if (( retval != 0 )); then
	return 1
fi

InstallLogger
retval=$?
if (( retval != 0 )); then
	return 1
fi

return 0
