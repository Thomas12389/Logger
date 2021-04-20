#ifndef __CONVERT_DATA_H_
#define __CONVERT_DATA_H_

#include <map>
#include <inttypes.h>
#include <mutex>
#include <algorithm>

struct Out_Message {
    double dPhyVal;
    std::string strPhyUnit;
    std::string strPhyFormat;
};

typedef std::map<std::string, Out_Message> MAP_Name_Message;

struct stu_Message {
	uint64_t msg_time;
	MAP_Name_Message msg_map;
	// stu_Message(const stu_Message& T) {
	// 	this->msg_time = T.msg_time;
		// 不可用， std::copy 必须要有 push_back() 方法
	// 	std::copy(T.msg_map.begin(), T.msg_map.end(), this->msg_map);
	// }
};

typedef struct {
	stu_Message msg_struct;
	std::mutex msg_lock;
} stu_SaveMessage, stu_OutMessage;

int can_dbc_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData);
int can_ccp_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData);
int can_obd_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData);

int write_msg_out_map(const std::string strOutName, const Out_Message& MsgOut);
int write_msg_save_map(const std::string strSaveName, const Out_Message& MsgSave);

#endif