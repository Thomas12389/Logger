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
	7		权限错误
			其他错误
!

# 检查权限
if (( $(id -u) != 0 )); then
	echo "Permission denied"
    exit 7
fi

# 安装脚本路径
this_file_path=$(cd "$(dirname "${BASH_SOURCE[0]}")" || exit; pwd)
# 公共文件路径
common_file_path=${this_file_path}/files/common
# 更新脚本所在绝对路径
update_script_path=${this_file_path}
# 更新脚本名称
update_script_name=update_fireware.sh
# 安装路径
install_path=/opt/DL_Manager
# 更新脚本安装路径
update_script_install_path=/opt/DL_Manager

# 更新包存放位置
fireware_path=/opt/DL_Fireware
# 更新包名称
fireware_name=Fireware.img

# 可执行程序软连接
softlink_tcp=/usr/bin/WINDHILL_TCP
softlink_wcp=/usr/bin/WINDHILL_WCP
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

function AutoRun() {

	local service_path=/etc/systemd/system
	local service_name=rc-local.service
	local service_rclocal=/etc/rc.local
	local service_script=/home/debian/TBox.sh
	local service_status

	# 产生服务文件
	if [ ! -f "${service_path}/${service_name}" ]; then
		echo "service file \"${service_path}/${service_name}\" not exists"
cat > "${service_path}/${service_name}" << EOF
[Unit]
After=pmsrv.service
Description=${service_rclocal} 
ConditionPathExists=${service_rclocal} 

[Service]
Type=forking
ExecStartPre=/bin/sleep 3
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
if [ -f "/var/lock/subsys/wh-wcp" ]; then
#	rm -rf /var/lock/subsys/wh-wcp
	exit 0
else
	touch /var/lock/subsys/wh-wcp
	/bin/bash ${service_script}
fi
exit 0
EOF
		chmod a+x ${service_rclocal}
	fi

	# 产生自定义脚本文件
	if [ ! -x ${service_script} ]; then
		echo "service script \"${service_script} \" not exists"
cat > ${service_script} << EOF
#!/bin/sh
	
	if [ "\$(ps aux | grep -v "grep" | grep -c "WINDHILL_WCP")" -eq 0 ]; then
		if [ -x "${softlink_wcp}" ]; then
			echo "start \"${softlink_wcp}\"."
			${softlink_wcp} > /dev/null 2>&1 &
		fi
	fi
	if [ "\$(ps aux | grep -v "grep" | grep -c "WINDHILL_TCP")" -eq 0 ]; then
		${softlink_tcp} > /dev/null 2>&1 &
	fi

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

	tar zxf sd-mount-0.0.1.tgz -C /
	tar zxf usb-mount-0.0.1.tgz -C /
	systemctl daemon-reload
	udevadm control --reload-rules

	cd "${cur_dir}" || return 1
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

# 安装 DL_Manager 前的准备
function InstallManagerPro() {

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
    if [[ ! -d ${install_path} ]]; then
        printf "mkdir install path: %s\n" "${install_path}"
        mkdir -p "${install_path}" || return 1
    fi

	if [ ! -d "${fireware_path}" ]; then
		printf "mkdir fireware path: %s\n" "${fireware_path}"
        mkdir -p "${fireware_path}" || return 1
	fi
	
	return 0
}

# 安装
function InstallManager() {

	local pro_path
	local pro_tcp_exe
	local is_pro_run
	# TODO：路径可能会更改
	pro_path=${install_file_dir}
	pro_tcp_exe=${pro_path}/WINDHILL_TCP

	is_pro_run=$(ps aux | grep -v "grep" | grep -c "WINDHILL_TCP")
	if (( is_pro_run != 0 )); then
		echo "WINDHILL_TCP is running."
		return 6
	fi

	# tcp 管理程序
	# if [ ! -x "${pro_tcp_exe}" ]; then
	# 	echo "file \"${pro_tcp_exe}\" is not executable"
	# 	return 3
	# fi
	cp "${pro_tcp_exe}" ${install_path} || return 3

	local exe_real_path="${install_path}"/WINDHILL_TCP
	chmod a+x ${exe_real_path}

	# 建立软连接
	if [ ! -h "${softlink_tcp}" ]; then
		ln -s "${exe_real_path}" ${softlink_tcp}
	fi
	
	return 0
}

# 安装后的其他事情
function InstallManagerPost() {

	local ret

	# 复制更新脚本
	if [ ! -e "${update_script_install_path}" ]; then
		echo "Path \"${update_script_install_path}\" does not exist"
		mkdir -p ${update_script_install_path} || return 1
	fi
	
	cp "${update_script_path}/${update_script_name}" "${update_script_install_path}" || return 1
	chmod +x "${update_script_install_path}/${update_script_name}" || return 1

	AutoMount
	AutoRun
	ret=$?
	if (( ret != 0 )); then
		return ${ret}
	fi
	
	SetPowerLED
}


InstallManagerPro
retval=$?
if (( retval != 0 )); then
	exit 1
fi

InstallManager
retval=$?
if (( retval != 0 )); then
	exit 1
fi

# 安装成功提示
whiptail --title "Tips" --msgbox "Install Success.\n" 10 60

InstallManagerPost
retval=$?
if (( retval != 0 )); then
	exit 1
fi

whiptail --title "Tips" --msgbox "Set auto run OK.\n" 10 60

reboot

exit 0
