#! /bin/bash
config_abs_path="/home/debian/WINDHILLRT/"

config_hw_abs_path=${config_abs_path}"hw.xml"
log_path="/home/debian/log/"
data_path="/media/TFM_DATA/"

# 遍历数据文件夹，找到未压缩的数据文件压缩
data_floder_list=$(find ${data_path} -maxdepth 1 -type d -regex "^.+[0-9_]$")
for dir in ${data_floder_list}; do
    #echo "${dir}"
    gzip -r "${dir}"    # 每个数据文件单独压缩，gzip 不支持多个文件压缩进同一个文件

    data_file_list=$(ls "${dir}")    # 压缩后的所有数据文件

    #找到对应的配置文件及日志文件
    log_file_name="log_""$(basename "${dir}")"".log"
    # 拷贝配置
    config_hw_last_name="description.xml"
    config_hw_last_path=${data_path}"/description.xml"
    cp ${config_hw_abs_path} ${config_hw_last_path}

    # 配置、数据、日志一起打包
    owa_file_name="${dir}"".owa"
    if [ -f "${owa_file_name}" ]; then
        echo "File \"${owa_file_name}\" exists"
        rm -rf "${owa_file_name}"
    fi
    
    # -c：创建  -z：使用 gzip 算法  -P：使用绝对路径    -f：打包文件名
    # -C：目标目录
    # ${data_file_list} 不可加双引号，否则文件名之间以 '\n' 分割，导致压缩失败
    tar -czPf  "${owa_file_name}" -C "${dir}" ${data_file_list} -C "${data_path}" "${config_hw_last_name}" -C ${log_path} "${log_file_name}" --remove-files

    # 删除该数据文件夹
    rm -rf "${dir}"
done

:<<!
# 若存在 temp 配置文件(上位机下载的配置)，则将其改为正式配置文件
config_new_hw=${config_abs_path}"temp_hw.xml"
config_new_app=${config_abs_path}"temp_app.json"
config_new_info=${config_abs_path}"temp_info.json"
   
if [ -f ${config_new_hw} ]; then
    mv ${config_new_hw} ${config_abs_path}"hw.xml"
fi
if [ -f ${config_new_app} ]; then
    mv ${config_new_app} ${config_abs_path}"app.json"
fi
if [ -f ${config_new_info} ]; then
    mv ${config_new_info} ${config_abs_path}"info.json"
fi
!


# 删除日志文件夹中超过 10 天的日志
find ${log_path} -type f -mtime +10 -name "*.log" -exec rm -rf {} \;
# 删除数据文件夹中超过 10 天的未压缩数据
find ${data_path} -maxdepth 1 -type d -mtime +10 -regex "^.+[0-9_]$" -exec rm -rf {} \;
# 删除数据文件夹中超过 30 天的压缩数据
find ${data_path} -maxdepth 1 -type d -mtime +30 -name "*.owa" -exec rm -rf {} \;
