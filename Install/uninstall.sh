#! /bin/bash

# 检查权限
if [[ ${UID} -ne 0 ]]; then
    echo "Permission denied"
    exit 0
fi