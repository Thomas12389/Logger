#! /bin/bash

config_hw_abs_path="/home/debian/WINDHILLRT/hw.xml"
log_path="/home/debian/log/"
data_path="/media/TFM_DATA/"
can_traffic_path=${data_path}"can_traffic/"

run_flag=0


function DEVICE_STATUS_COMPRESS() {
    echo 0 > /sys/class/leds/ledsw2\\:red/brightness
    echo timer > /sys/class/leds/ledsw2\\:red/trigger
    echo 800 > /sys/class/leds/ledsw2\\:red/delay_on
    echo 200 > /sys/class/leds/ledsw2\\:red/delay_off
}

function DEVICE_STATUS_OFF() {
    echo 0 > /sys/class/leds/ledsw2\\:red/brightness
}

# $1: 文件夹绝对路径
# $2: 旧后缀
# $3: 新后缀
function modify_suffix() {
    if (( $# != 3 )); then
        return 1
    fi
    # 文件夹路径不存在
    if [ ! -d "$1" ]; then
        return 2
    fi
    # 新、旧后缀相同，认为修改成功
    if [[ "$2" == "$3" ]]; then
        echo "The two suffix are the same"
        return 0
    fi
    
    
    current_dir=$(pwd)
    cd "$1" || return 3
    for file in $(ls "$1" | grep ."$2")
    do
        name=$(ls "${file}" | cut -d . -f 1)
        mv "${file}" "${name}"."$3"
    done
    cd "${current_dir}" || return 3
}

# 遍历数据文件夹，找到未压缩的数据文件夹(绝对路径)压缩
data_floder_list=$(find ${data_path} -maxdepth 1 -type d -regex "^.+[0-9_]$")

# 存在数据文件夹
if [[ -n "${data_floder_list}" ]]; then
    # echo "data_floder_list is not empty"
    run_flag=1
fi

if (( run_flag == 1 )); then
    DEVICE_STATUS_COMPRESS
    for dir in ${data_floder_list}; do
        # echo "${dir}"
        data_gz_file="${dir}"".owa"    # 数据文件压缩包
        if [ -f "${data_gz_file}" ]; then
            # echo "File \"${data_gz_file}\" exists"
            rm -rf "${data_gz_file}"
        fi

        owa_file_name="${dir}"".tgz"    # 最终打包文件名称
        if [ -f "${owa_file_name}" ]; then
            # echo "File \"${owa_file_name}\" exists"
            rm -rf "${owa_file_name}"
        fi
        # 若数据文件夹不为空，则压缩其中的数据文件
        data_file_list=$(ls -A "${dir}")    # 压缩前的数据文件列表
        if [[ -n "${data_file_list}" ]]; then
            gzip -r "${dir}"    # 每个数据文件单独压缩，gzip 不支持多个文件压缩进同一个文件
            modify_suffix "${dir}" "gz" "owa"
        fi

        data_file_list=$(ls "${dir}")    # 压缩后的数据文件列表

        #找到对应的日志文件
        log_file_name="log_""$(basename "${dir}")"".log"

        # 找到对应的 can_traffic 文件
        can_blf_file_name="$(basename "${dir}")"".blf"

        # 配置、数据、日志一起打包
        # -c：创建  -z：使用 gzip 算法  -P：使用绝对路径    -f：打包文件名
        # -C：目标目录
        # ${data_file_list} 不可加双引号，否则文件名之间以 '\n' 分割，导致压缩失败

        tar_cmd="tar -czPf ${owa_file_name}"
        # data_file_list 不为空，则加入打包压缩
        if [[ -n "${data_file_list}" ]]; then
            echo "${data_file_list}"
            tar_cmd="${tar_cmd}"" -C ${dir} ${data_file_list}"

            # 拷贝对应的配置
            config_hw_last_name="description.xml"
            config_hw_last_path=${data_path}"/description.xml"
            cp ${config_hw_abs_path} ${config_hw_last_path}
            tar_cmd="${tar_cmd}"" -C ${data_path} ${config_hw_last_name}"
        fi
        
        # 检查 traffic 文件是否存在
        if [ -f "${can_traffic_path}${can_blf_file_name}" ]; then
            data_time=${dir##*/}    # 剔除最后一个 ‘/’ 及左边的所有字符
            can_blf_file_list=$(ls "${can_traffic_path}" | grep "${data_time}" | tr "\n" " ")
            tar_cmd="${tar_cmd}"" -C ${can_traffic_path} ${can_blf_file_list}"
        fi

        # 检查 log 文件是否存在
        if [ -f "${log_path}${log_file_name}" ]; then
            tar_cmd="${tar_cmd}"" -C ${log_path} ${log_file_name}"
        fi       

        tar_cmd="${tar_cmd}"" --remove-files"
        # 加双引号可能会导致 ‘\n’，使得命令无法执行
        eval ${tar_cmd}

        # 删除该数据文件夹
        rm -rf "${dir}"
    done
    DEVICE_STATUS_OFF
fi

# 删除日志文件夹中超过 5 天的日志
find ${log_path} -type f -mtime +5 -name "*.log" -exec rm -rf {} \;

# 删除数据文件夹中超过 5 天的未压缩数据
find ${data_path} -maxdepth 1 -type d -mtime +5 -regex "^.+[0-9_]$" -exec rm -rf {} \;
# 删除数据文件夹中超过 5 天的临时压缩数据
find ${data_path} -maxdepth 1 -type f -mtime +5 -name "*.owa" -exec rm -rf {} \;
# 删除数据文件夹中超过 30 天的压缩数据
find ${data_path} -maxdepth 1 -type f -mtime +30 -name "*.tgz" -exec rm -rf {} \;

# 删除 traffic 文件夹中超过 5 天的 traffic 文件
find ${can_traffic_path} -type f -mtime +5 -name "*.blf" -exec rm -rf {} \;

if (( run_flag == 1 )); then
    exit 0
else 
    exit 1
fi


