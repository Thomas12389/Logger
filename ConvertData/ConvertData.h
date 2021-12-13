#ifndef __CONVERT_DATA_H_
#define __CONVERT_DATA_H_

#include <inttypes.h>
#include <mutex>
#include <algorithm>

#include <list>

struct Out_Message {
	std::string strName;
    double dPhyVal;
    std::string strPhyUnit;
    std::string strPhyFormat;
};

typedef std::list<Out_Message> LIST_Message;

struct stu_Message {
	uint64_t msg_time;
	LIST_Message msg_list;
};

typedef struct {
	stu_Message msg_struct;
	std::mutex msg_lock;
} stu_SaveMessage, stu_OutMessage;

int can_dbc_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData);
int can_ccp_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData);
#ifdef SELF_OBD
int can_obd_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData);
#endif

int write_msg_out_map(const std::string strOutName, const Out_Message& MsgOut);
int write_msg_save_map(const std::string strSaveName, const Out_Message& MsgSave);

#endif