
#ifndef __DL_ERRNO_H__
#define __DL_ERRNO_H__


#define COMMON_BASE                     0x0000
#define SUCCESS                         COMMON_BASE + 0x01
#define FAIL                            COMMON_BASE + 0x02
#define PAYLOAD_LENGTH_ERR              COMMON_BASE + 0x03
#define UNKNOWN_CMD                     COMMON_BASE + 0xFF

#define UPLOAD_ERR_BASE                 0x0100
#define UNSUPPORTED_UL_TYPE             UPLOAD_ERR_BASE + 0x01

#define DOWNLOAD_ERR_BASE               0x0200
#define UNSUPPORTED_DL_TYPE             DOWNLOAD_ERR_BASE + 0x01
#define DL_FILE_LEN_ERR                 DOWNLOAD_ERR_BASE + 0x02

#define TIME_RELEATED_ERR_BASE          0x0300
#define INCALID_TIMEZONE                TIME_RELEATED_ERR_BASE + 0x01
#define INCALID_TIME                    TIME_RELEATED_ERR_BASE + 0x02

#define UPDATE_ERR_BASE                 0x0400
#define PERMISSSION_DENIED              UPDATE_ERR_BASE + 0x01
#define DECOMPRESSION_ERR               UPDATE_ERR_BASE + 0x02
#define UNINSTALL_ERR                   UPDATE_ERR_BASE + 0x03
#define INSTALL_ERR                     UPDATE_ERR_BASE + 0x04

#endif
