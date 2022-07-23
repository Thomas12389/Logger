
#ifndef __TFM_COMMON_H__
#define __TFM_COMMON_H__

#include <inttypes.h>
#include <string>

#define __DEBUG
#ifdef __DEBUG
#define DEBUG_PTINTF(format, ...) printf("[" __FILE__ "] [line: %d]\t" format, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PTINTF
#endif

/* 
NAME	表示文件的名称，不带路径
DIR 	表示某一个文件夹，最后有 “/”
PATH	表示文件的绝对路径
 */
#ifdef PC_TEST
const std::string FLASH_MOUNT_DIR = "/";
const std::string TF_MOUNT_DIR = "/media/windhill/Ventoy/";
const std::string DATA_DIR = TF_MOUNT_DIR;
const std::string USER_LOG_DIR = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/log/";
const std::string TEMP_HW_XML = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/config/temp_hw.xml";
const std::string TEMP_APP_JSON = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/config/temp_app.json";
const std::string TEMP_INFO_JSON = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/config/temp_info.json";
const std::string REAL_HW_XML = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/config/hw.xml";
const std::string REAL_APP_JSON = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/config/app.json";
const std::string REAL_INFO_JSON = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/config/info.json";
const std::string REAL_LICENSE = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/config/license";
const std::string REAL_FIREWARE = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/fireware.img";
const std::string SYS_LOG_PATH = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/";
const std::string SYS_LOG_NAME = "sys.log";

const std::string START_SCRIPT = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/test.sh";
const std::string UPDATE_SCRIPT = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/test_shell/update_fireware.sh";

const std::string FIFO_REQ_PATH = "/tmp/LoggerRequest";
const std::string FIFO_RSP_PATH = "/tmp/LoggerResponse";

const std::string WCP_INFO_INI_PATH = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/DL_Logger/info.ini";
const std::string TCP_INFO_INI_PATH = "/home/windhill/Desktop/Remote_Recoder/WINDHILL_TCP/DL_Manager/info.ini";
#else
const std::string WCP_NAME = "WINDHILL_WCP";
/* 存储器挂载点 */
const std::string FLASH_MOUNT_DIR = "/";
const std::string TF_MOUNT_DIR = "/media/TFM_DATA/";
/* 数据文件目录 */
const std::string DATA_DIR = TF_MOUNT_DIR;
/* 用户日志， wcp 日志 */
const std::string USER_LOG_DIR = "/home/debian/log/";
const std::string USER_LOG_NAME = "user.log";	// 仅用于上传时临时文件名
/* traffic 目录 */
const std::string CAN_TRAFFIC_DIR = TF_MOUNT_DIR + "can_traffic/";
/* 配置文件路径 */
const std::string CONFIG_DIR = "/home/debian/WINDHILLRT/";
const std::string TEMP_HW_XML_NAME = "temp_hw.xml";
const std::string TEMP_APP_JSON_NAME = "temp_app.json";
const std::string TEMP_INFO_JSON_NAME = "temp_info.json";
const std::string TEMP_LICENSE_NAME = "temp_license.ini";
const std::string REAL_HW_XML_NAME = "hw.xml";
const std::string REAL_APP_JSON_NAME = "app.json";
const std::string REAL_INFO_JSON_NAME = "info.json";
const std::string REAL_LICENSE_NAME = "license.ini";
const std::string TEMP_HW_XML_PATH = CONFIG_DIR + TEMP_HW_XML_NAME;
const std::string TEMP_APP_JSON_PATH = CONFIG_DIR + TEMP_APP_JSON_NAME;
const std::string TEMP_INFO_JSON_PATH = CONFIG_DIR + TEMP_INFO_JSON_NAME;
const std::string TEMP_LICENSE_PATH = CONFIG_DIR + TEMP_LICENSE_NAME;
const std::string REAL_HW_XML_PATH = CONFIG_DIR + REAL_HW_XML_NAME;
const std::string REAL_APP_JSON_PATH = CONFIG_DIR + REAL_APP_JSON_NAME;
const std::string REAL_INFO_JSON_PATH = CONFIG_DIR + REAL_INFO_JSON_NAME;
const std::string REAL_LICENSE_PATH = CONFIG_DIR + REAL_LICENSE_NAME;
/* 固件路径 */
const std::string FIREWARE_DOWNLOAD_DIR = "/opt/DL_Fireware/";
const std::string REAL_FIREWARE_NAME = "Fireware.img";
const std::string REAL_FIREWARE_PATH = FIREWARE_DOWNLOAD_DIR + REAL_FIREWARE_NAME;

const std::string WCP_INSTALL_DIR = "/opt/DL_Logger/";
const std::string TCP_INSTALL_DIR = "/opt/DL_Manager/";
const std::string SYS_LOG_NAME = "sys.log";
const std::string SYS_LOG_DIR = TCP_INSTALL_DIR;
const std::string SYS_LOG_PATH = SYS_LOG_DIR + SYS_LOG_NAME;
/* 脚本文件路径 */
const std::string TFM_SCRIPT_DIR = "/opt/TFM_scripts/";
const std::string DEAL_OLD_FILES_SCRIPT_PATH = TFM_SCRIPT_DIR + "deal_old_files.sh";
const std::string WLAN_STOP_SCRIPT_PATH = TFM_SCRIPT_DIR + "WLAN_stop.sh";
const std::string WLAN_START_SCRIPT_PATH = TFM_SCRIPT_DIR + "WLAN_start.sh";
// 不要修改
const std::string START_SCRIPT_PATH = "/home/debian/TBox.sh";
const std::string UPDATE_FIREWARE_SCRIPT_NAME = "update_fireware.sh";
const std::string UPDATE_FIREWARE_SCRIPT_PATH = TCP_INSTALL_DIR + UPDATE_FIREWARE_SCRIPT_NAME;
const std::string WCP_START_SCRIPT_NAME = "wcp_start.sh";
const std::string WCP_START_SCRIPT_PATH = TCP_INSTALL_DIR + WCP_START_SCRIPT_NAME;
//

/* 版本信息文件路径 */
const std::string INFO_INI_NAME = "info.ini";
const std::string WCP_INFO_INI_PATH = WCP_INSTALL_DIR + INFO_INI_NAME;
const std::string TCP_INFO_INI_PATH = TCP_INSTALL_DIR + INFO_INI_NAME;
const std::string FIREWARE_TEMP_DIR = FIREWARE_DOWNLOAD_DIR + "temp/";
const std::string NEW_WCP_INFO_INI_PATH = FIREWARE_TEMP_DIR + INFO_INI_NAME;
/* 进程通信 fifo */
const std::string FIFO_REQ_PATH = "/tmp/LoggerRequest";
const std::string FIFO_RSP_PATH = "/tmp/LoggerResponse";
#endif

uint64_t my_ntoh64(uint64_t __net64);
uint64_t my_hton64(uint64_t __host64);

float my_ntohf(float __hostf);
float my_htonf(float __hostf);

/* 返回值 
	-1：	获取状态出错
	0：		检测的程序未运行
	1：		检测的程序在运行
 */
extern int get_process_status(const char* proname);

int my_system(const char *cmd);

#endif