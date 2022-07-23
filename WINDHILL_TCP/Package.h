/*
 * @Author: your name
 * @Date: 2022-01-25 15:54:50
 * @LastEditTime: 2022-06-06 15:37:08
 * @LastEditors: Please set LastEditors
 * @Description: 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 * @FilePath: /Remote_Recoder/slave_tcp/Package.h
 */
/*************************************************************************
	> File Name: Package.h
	> Author: 
	> Mail: 
	> Created Time: Mon 08 Nov 2021 05:59:38 PM CST
 ************************************************************************/

#ifndef _PACKAGE_H
#define _PACKAGE_H

#include <string>

#include "DL_errno.h"

const uint8_t HEADER_LENGTH = 8;
const uint8_t PRO_VERSION = 0x02;
const uint8_t CMD_PARA_LENGTH = 2;

typedef enum CMD_ID : uint16_t {
	MIN_CMD_ID = 0x0000,
	Get_Version_Info = 0x0001,
	Download = 0x0002,
	Upload_Regualr_File = 0x0003,
	Upload_Data_File = 0x0004,
	Delete_Data_File = 0x0005,
	Get_Data_File_Info = 0x0006,
	Logger_Action = 0x0007,
	Method_System_Time = 0x0008,
	Method_TZ = 0x0009,
	Update_Fireware = 0x000A,
	Get_Status = 0x000B,
	MAX_CMD_ID = 0x000C

} CMD_ID;

typedef struct Package_Header {
	uint8_t u8_pro_version;
	uint8_t u8_inv_version;
	CMD_ID u16_cmd_id;
	uint32_t u32_data_len;
} Package_Header;

typedef enum DOWNLOAD_TYPE : uint16_t {
	dl_unknown_type = 0x0000,
	dl_xml_hw = 0x0001,
	dl_json_app = 0x0002,
	dl_json_info = 0x0003,
	dl_fireware = 0x0004,
	dl_license = 0x0005
} DOWNLOAD_TYPE;

typedef enum UPLOAD_TYPE : uint16_t {
	ul_unknown_type = 0x0000,
	ul_xml_hw = 0x0001,
	ul_user_log = 0x0002,
	ul_sys_log = 0x0003,
	ul_license = 0x0004
} UPLOAD_TYPE;

typedef enum LOGGER_ACTION : uint16_t {
	act_start = 0x0001,
	act_stop = 0x0002,
	act_reset = 0x0003
} LOGGER_ACTION;

typedef enum METHOD : uint16_t {
	get = 0x0001,
	set = 0x0002
} METHOD;

typedef enum GET_STATUS : uint16_t {
	get_sys_status = 0x0001,
	get_wcp_status = 0x0002
} GET_STATUS;

typedef enum RESPONSE_ID : uint16_t {
	rsp_common_base = 0x0000,
	rsp_common_success = rsp_common_base + 0x01,
	rsp_common_fail = rsp_common_base + 0x02,
	rsp_common_payload_len_error = rsp_common_base + 0x03,
	rsp_common_get_file_attr_error = rsp_common_base + 0x04,
	rsp_common_timeout = rsp_common_base + 0x05,
	rsp_common_open_failed = rsp_common_base + 0x06,
	rsp_common_script_not_exist = rsp_common_base + 0x07,
	rsp_common_unknown_cmd = rsp_common_base + 0xFF,

	rsp_upload_error_base = 0x0100,
	rsp_upload_unsupported_type = rsp_upload_error_base + 0x01,
	rsp_upload_file_not_exist = rsp_upload_error_base + 0x02,

	rsp_download_error_base = 0x0200,
	rsp_download_unsupported_type = rsp_download_error_base + 0x01,
	rsp_download_file_len_error = rsp_download_error_base + 0x02,
	rsp_download_file_save_error = rsp_download_error_base + 0x03,
	rsp_download_file_length_zero = rsp_download_error_base + 0x04,
	rsp_download_config_dir_not_exist = rsp_download_error_base + 0x05,

	rsp_time_related_error_base = 0x0300,
	rsp_time_invalid_timezone = rsp_time_related_error_base + 0x01,
	rsp_time_invalid_time = rsp_time_related_error_base + 0x02,
	rsp_time_unknown_method = rsp_time_related_error_base + 0x03,
	rsp_time_wcp_is_running = rsp_time_related_error_base + 0x04,

	rsp_update_error_base = 0x0400,
	rsp_update_decompression_error = rsp_update_error_base + 0x01,
	rsp_update_uninstall_error = rsp_update_error_base + 0x02,
	rsp_update_install_error = rsp_update_error_base + 0x03,
	rsp_update_isrunning_error = rsp_update_error_base + 0x04,
	rsp_update_version_error = rsp_update_error_base + 0x05,
	rsp_update_get_wcp_version_error = rsp_update_error_base + 0x06,
	rsp_update_get_tcp_version_error = rsp_update_error_base + 0x07,

	rsp_action_error_base = 0x0500,
	rsp_action_has_execute_start = rsp_action_error_base + 0x01,
	rsp_action_has_execute_stop = rsp_action_error_base + 0x02,
	rsp_action_is_running = rsp_action_error_base + 0x03,
	rsp_action_is_stopped = rsp_action_error_base + 0x04,
	rsp_action_unsupported = rsp_action_error_base + 0x05,
	rsp_action_reset_failed = rsp_action_error_base + 0x06,

	rsp_get_status_error_base = 0x0600,
	rsp_get_status_cpuload_error = rsp_get_status_error_base + 0x01,
	rsp_get_status_ram_error = rsp_get_status_error_base + 0x02,
	rsp_get_status_flash_error = rsp_get_status_error_base + 0x03,
	rsp_get_status_tf_error = rsp_get_status_error_base + 0x04,
	rsp_get_status_wcp_error = rsp_get_status_error_base + 0x05,
	rsp_get_status_unsupported = rsp_get_status_error_base + 0x06,

	rsp_get_version_error_base = 0x0700,
	rsp_get_version_serial_error = rsp_get_version_error_base + 0x01,
	rsp_get_version_sysfw_error = rsp_get_version_error_base + 0x02,
	rsp_get_version_wcp_error = rsp_get_version_error_base + 0x03,
	rsp_get_version_tcp_error = rsp_get_version_error_base + 0x04,
} RSP_ID;

inline bool is_need_cmdpara(CMD_ID cmd_id) {
	if ( (cmd_id == CMD_ID::Get_Version_Info) || (cmd_id == CMD_ID::Get_Data_File_Info) || 
			(cmd_id == CMD_ID::Update_Fireware) ) {
		return false;
	}

	return true;
}

#endif
