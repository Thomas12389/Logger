#! /bin/bash

:<<!
参数说明：
	$0		该脚本文件名
	$1		SSID
	$2		密码
    $3		加密方式，目前不用
!

if (( $# != 2 )); then
    exit 1
fi

# wpa 配置文件路径
wpa_path=/etc/wpa.conf
# 生成配置文件
cat > ${wpa_path} << EOF
ctrl_interface=DIR=/var/run/wpa_supplicant
update_config=1
#
# allow all valid ciphers
network={
    ssid="$1"
    psk="$2"
    scan_ssid=1
}

EOF

# 使能 WiFi/BT 模块
if ! Start_BT_WiFi 1; then
    exit 1
fi

until [ "$(ifconfig -a | grep -c "mlan0")" -eq 1 ]; do
    sleep 1s
done

# 开启接口
if ! ip link set mlan0 up; then
    exit 2
fi
# 开启 wpa_supplicant
# -B:后台运行  -s:输出到 syslog
if ! wpa_supplicant -i mlan0 -c ${wpa_path} -B -s; then
    exit 3
fi

until [ "$(ps aux | grep -v "grep" | grep -c "wpa_supplicant -i mlan0")" -eq 1 ]; do
    sleep 1s
done

# 清除 IP 信息
ip addr flush dev mlan0
# 自动获取 IP
dhclient mlan0
