#! /bin/bash

# 清除 IP 信息
if [ "$(ifconfig -a | grep -c "mlan0")" -eq 1 ]; then
    ip addr flush dev mlan0
fi

wpa_pid=$(pidof wpa_supplicant)

if (( 0 == $? )); then
    for id in ${wpa_pid}
    do
        kill -s SIGINT "${id}"
    done
fi

# 关闭接口
if [ "$(ifconfig -a | grep -c "mlan0")" -eq 1 ]; then
    ip link set mlan0 down
fi
# 禁能 WiFi/BT 模块
Start_BT_WiFi 0
