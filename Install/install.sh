#! /bin/bash

# 检查权限
if [[ ${UID} -ne 0 ]]; then
    echo "Permission denied"
    exit 0
fi

# 安装路径
InstallPath=/opt/DL_Logger

# 安装 DL_Logger 前的准备
function InstallLoggerPro() {
    local bChangeInstallPath="N"
    read -rp "The default install directory is ${InstallPath}, would you like to change it?(Y/N, default:N):" bChangeInstallPath
    printf "\n"

    # 更改安装目录
    if [[ ${bChangeInstallPath} == "Y" || ${bChangeInstallPath} == "y" ]]; then
        read -rp "Please enter the install path:" InstallPath
        printf "\n"
    fi

    # 安装目录不存在
    if [[ ! -e ${InstallPath} ]]; then
        # printf "install path: %s\n" "${InstallPath}"
        mkdir -p "${InstallPath}"
    fi

    # 拷贝文件

}

InstallLoggerPro

exit 0
