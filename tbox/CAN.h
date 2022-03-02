
#ifndef __CAN_H_
#define __CAN_H_

#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cinttypes>

#ifndef CAN_NUM
#define CAN_NUM 4
#endif

#ifndef ENABLE_CAN
#define ENABLE_CAN 1
#endif

#ifndef DISABLE_CAN
#define DISABLE_CAN 0
#endif
	
#define MAX_SIZE	255

typedef struct CAN_CHOOSE
{
	int nNumCan;
	char wCan[CAN_NUM];	//wCan[i]	CAN(i+1)
						//	1		  使用
						//	0		  不使用
} CAN_CHOOSE;

struct canInfo {
	int nFd;
	char *nChanName;
};

// 协议类型
enum protocol_type : uint8_t {
	PRO_DBC = 0x01,
	PRO_CCP = 0x02,
	PRO_OBD = 0x03,
	PRO_WWHOBD = 0x04,
	PRO_J1939 = 0x05,
	PRO_UDS = 0x06,
	PRO_XCP = 0x07,
};

// ID 注册
struct RegInfo{
	std::string fifo_path;
	std::vector<uint32_t> reg_ids;
};
typedef std::map<protocol_type, std::vector<RegInfo> > mapProtype_IDs;
typedef std::map<std::string, mapProtype_IDs> mapID_Register;	// string 通道名称

// CAN 接收缓冲区
struct CANReceive_Buffer {
	// uint64_t rcv_timestamp;
	uint32_t	can_id;		/* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t		can_dlc;
	uint8_t		flags;		/* only for CANFD */
	uint8_t		__res0;		/* reserved / padding */
	uint8_t		__res1;		/* reserved / padding */
	uint8_t		can_data[64] __attribute__((aligned(8)));
};

typedef std::unordered_map<uint32_t, CANReceive_Buffer> CAN_UOMAP_MsgID_Buffer;
typedef std::unordered_map<std::string, CAN_UOMAP_MsgID_Buffer> CAN_UOMAP_ChanName_RcvPkg;

typedef struct stu_CAN_UOMAP_ChanName_RcvPkg {
	CAN_UOMAP_ChanName_RcvPkg can_buffer;
	std::mutex can_buffer_lock;
} stu_CAN_UOMAP_ChanName_RcvPkg;


int Can_Enable(int);
int Can_Start(void);
void Can_Stop(void);

int can_send(std::string chanName, int nFd, uint32_t id, uint8_t len, uint8_t *byteArray);

int can_register(std::string chn_name, protocol_type type, RegInfo reginfo);

const char* protocol_enum_to_string(protocol_type type);
	
#endif




