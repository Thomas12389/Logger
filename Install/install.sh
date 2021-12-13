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
	255		其他错误
!

# 检查权限
if (( $(id -u) != 0 )); then
	echo "Permission denied"
    exit 0
fi

# 安装脚本路径
this_file_path=$(cd "$(dirname "${BASH_SOURCE[0]}")" || exit; pwd)
# 公共文件路径
common_file_path=${this_file_path}/files/common
# 默认安装路径
default_install_path=/opt/DL_Logger
install_path=${default_install_path}
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

# 拷贝库文件
function CopyLibFiles() {

	local lib_target_dir=/usr/lib
	local lib_source_dir
	local cur_dir
	local lib_files_list="libssh2.so.1.0.1 libcrypto.so.1.0.0 libssl.so.1.0.0 libmosquitto.so.1"

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
		
		cp "${lib_file}" ${lib_target_dir}
	done

	cd "${lib_target_dir}" || return 1
	# 创建 libssh2 的软连接
	if [ ! -h "libssh2.so.1" ]; then
		ln -s libssh2.so.1.0.1 libssh2.so.1
	fi
	cd "${cur_dir}" || return 0

	return 0
}

function AutoRun() {

	local service_path=/etc/systemd/system
	local service_name=rc-local.service		#wh-wcp.service
	local service_rclocal=/etc/rc.local
	local service_script=/home/debian/TBox.sh
	local service_status

	# 产生服务文件
	if [ ! -f "${service_path}/${service_name}" ]; then
		echo "service file \"${service_path}/${service_name}\" not exists"
cat > "${service_path}/${service_name}" << EOF
[Unit]
Description=${service_rclocal} 
ConditionPathExists=${service_rclocal} 

[Service]
Type=forking
ExecStart=${service_rclocal} start
TimeoutSec=0
StandardOutput=tty
RemainAfterExit=yes
SysVStartPriority=99

[Install]
WantedBy=multi-user.target
EOF
	fi
	
	# 产生 rc.local 脚本
	if [ ! -x ${service_rclocal} ]; then
		echo "service rclocal \"${service_rclocal} \" not exists"
cat > ${service_rclocal} << EOF
#!/bin/bash
#if [ -f "/var/lock/subsys/wh-wcp" ]; then
#	echo "File \"/var/lock/subsys/wh-wcp\" exists"
#	rm -rf /var/lock/subsys/wh-wcp
#	exit 0
#else
#	touch /var/lock/subsys/wh-wcp
	/bin/bash ${service_script}
#fi
exit 0
EOF
		chmod a+x ${service_rclocal}
	fi

	# 产生自定义脚本文件
	if [ ! -x ${service_script} ]; then
		echo "service script \"${service_script} \" not exists"
cat > ${service_script} << EOF
#!/bin/sh
#if [ -f "/tmp/.wh-wcp" ]; then
#	echo "File \"/tmp/.wh-wcp\" exists" >> /home/debian/xxxxx
#	rm -rf /var/lock/subsys/wh-wcp
#	exit 0
#else
#	touch /tmp/.wh-wcp
#	date >> /home/debian/xxxxx
#	sleep 1s
	/usr/bin/WINDHILL_WCP > /dev/null 2>&1 &
#fi

exit 0
EOF
		chmod a+x ${service_script}
	fi

	service_status=$(systemctl status ${service_name} | grep "Active" | awk -F":" '{print $2}' | awk -F" " '{print $1}')
	if [[ "${service_status}" == "active" ]]; then
		echo "\"${service_name}\" status: \"${service_status}\""
		return 0
	fi

	# 启动服务
	systemctl enable ${service_name}
	systemctl start ${service_name}
	
	service_status=$(systemctl status ${service_name} | grep "Active" | awk -F":" '{print $2}' | awk -F" " '{print $1}')
	if [[ "${service_status}" != "active" ]]; then
		echo "\"${service_name}\" status: \"${service_status}\""
		return 5
	fi

	return 0
}

function AutoMount() {

	local cur_dir
	local sd_service_name
	local usb_servicr_name
	
	cur_dir=$(pwd)

	cd "${common_file_path}" || return 1

	tar zxvf sd-mount-0.0.1.tgz -C /
	tar zxvf usb-mount-0.0.1.tgz -C /
	systemctl daemon-reload
	udevadm control --reload-rules

	cd "${cur_dir}" || return 1
}

# 设置时区
function SetTZone() {
	dpkg-reconfigure tzdata
}

function SetPowerLED() {

	local dtb_dir
	local dtb_file
	local dtb_local_file
	local led_set_file=/etc/power_led

	if [ -f "${led_set_file}" ]; then
		echo "File \"${led_set_file}\" exists"
		return 0
	fi

	# 设置电源指示灯
	dtb_dir=/boot/dtbs/$(uname -r)
	if [ ! -d "${dtb_dir}" ]; then
		echo "directory \"${dtb_dir}\" not exists."
		return 2
	fi

	dtb_file=${dtb_dir}/am335x-owasys.dtb
	dtb_local_file=${dtb_dir}/am335x-owasys-local.dtb

	if [ ! -f "${dtb_file}" ]; then
		return 3
	fi

	# 电源灯设置为常亮状态
	fdtput -ts "${dtb_file}" /leds/green default-state on || return 1

	if [ -f "${dtb_local_file}" ]; then
		rm "${dtb_local_file}"
	fi

	touch ${led_set_file}

	whiptail --title "Tips" --msgbox "After reboot, login into the system and wait 30 seconds so that the right dtb is generated." 10 60
	sleep 3s
	reboot
}

# 安装 DL_Logger 前的准备
function InstallLoggerPro() {

    local bChangeInstallPath="N"

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

	# 安装路径
	if (whiptail --title "Install Path" --yesno --defaultno "The default install directory is ${install_path},\
			would you like to change it?" 10 60) then
		
		bChangeInstallPath="Y"
	else
		bChangeInstallPath="N"
	fi

    # 更改安装目录
    if [[ ${bChangeInstallPath} == "Y" ]]; then
        install_path="$(whiptail --title "Change Install Path" --inputbox "Please enter the install path:" 10 60 3>&1 1>&2 2>&3)"
		# 选择 <Cancel>，则安装路径还原为默认路径
		if (( "$?" == 1 )); then
			install_path=${default_install_path}
		else
			# 选择了 <Ok>，但是输入的路径为空，则安装路径还原为默认路径
			if [[ -z "${install_path}" ]]; then
				install_path=${default_install_path}
			fi
		fi
    fi

    # 安装目录不存在
    if [[ ! -d ${install_path} ]]; then
        printf "mkdir install path: %s\n" "${install_path}"
        mkdir -p "${install_path}"
    fi

	return 0
}

# 安装
function InstallLogger() {

	local pro_path
	local pro_exe
	local is_pro_run
	# TODO：路径可能会更改
	pro_path=${install_file_dir}
	pro_exe=${pro_path}/WINDHILL_WCP

	is_pro_run=$(ps aux | grep -v "grep" | grep -c "WINDHILL_WCP")
	if (( is_pro_run != 0 )); then
		echo "WINDHILL_WCP is running."
		return 6
	fi
	

	if [ ! -x "${pro_exe}" ]; then
		echo "file \"${pro_exe}\" is not executable"
		return 3
	fi
	
	cp "${pro_exe}" ${install_path}
	# 建立软连接
	if [ ! -h "/usr/bin/WINDHILL_WCP" ]; then
		ln -s "${install_path}"/WINDHILL_WCP /usr/bin/WINDHILL_WCP
	fi

	CopyLibFiles
	ret=$?
	if (( ret != 0 )); then
		return ${ret}
	fi
	
	return 0
}

# 安装后的其他事情
function InstallLoggerPost() {

	local ret

	SetTZone
	AutoMount
	AutoRun
	ret=$?
	if (( ret != 0 )); then
		return ${ret}
	fi
	
	SetPowerLED
}


InstallLoggerPro
retval=$?
if (( retval != 0 )); then
	exit
fi

InstallLogger
retval=$?
if (( retval != 0 )); then
	exit
fi

# 安装成功提示
whiptail --title "Tips" --msgbox "Install Success.\n" 10 60

InstallLoggerPost
retval=$?
if (( retval != 0 )); then
	exit
fi

whiptail --title "Tips" --msgbox "Set auto run OK.\n" 10 60

reboot

exit 0
