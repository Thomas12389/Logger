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

typedef enum CMD_ID : uint16_t {
	MIN_CMD_ID = 0x0000,
	// Pre_Download = 0x0001,
	Get_Sys_Info = 0x0001,
	Download = 0x0002,
	// Pre_Upload_Data_File = 0x0003,
	Upload_Regualr_File = 0x0003,
	Upload_Data_File = 0x0004,
	Delete_Data_File = 0x0005,
	Get_Data_File_Info = 0x0006,
	Logger_Action = 0x0007,
	Method_System_Time = 0x0008,
	Method_TZ = 0x0009,
	Update_Fireware = 0x000A,
	MAX_CMD_ID = 0x000B

} CMD_ID;

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

typedef enum RESPONSE_ID : uint16_t {
	rsp_common_base = COMMON_BASE,
	rsp_success = rsp_common_base + 0x01,
	rsp_fail = rsp_common_base + 0x02,
	rsp_payload_len_error = rsp_common_base + 0x03,
	rsp_unknown_cmd = rsp_common_base + 0xFF,

	rsp_upload_error_base = 0x0100,
	rsp_unsupported_ul_type = rsp_upload_error_base + 0x01,

	rsp_download_error_base = 0x0200,
	rsp_unsupported_dl_type = rsp_download_error_base + 0x01,
	rsp_dl_file_len_error = rsp_download_error_base + 0x02,

	rsp_time_related_error_base = 0x0300,
	rsp_invalid_timezone = rsp_time_related_error_base + 0x01,
	rsp_invalid_time = rsp_time_related_error_base + 0x02,

	rsp_update_error_base = 0x0400,
	rsp_permission_denied = rsp_update_error_base + 0x01,
	rsp_decompression_error = rsp_update_error_base + 0x02,
	rsp_uninstall_error = rsp_update_error_base + 0x03,
	rsp_install_error = rsp_update_error_base + 0x04
} RSP_ID;

typedef enum LOGGER_ACTION : uint16_t {
	act_start = 0x0001,
	act_stop = 0x0002,
	act_reatsrt = 0x0003
} LOGGER_ACTION;

typedef enum METHOD : uint16_t {
	get = 0x0001,
	set = 0x0002
} METHOD;

typedef struct Package_Header {
	uint8_t u8_pro_version;
	uint8_t u8_inv_version;
	CMD_ID u16_cmd_id;
	uint32_t u32_data_len;
} Package_Header;


#endif
