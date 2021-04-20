
#ifndef __CAN_H_
#define __CAN_H_

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
	int wCan[CAN_NUM];	//wCan[i]	CAN(i+1)
						//	1		  使用
						//	0		  不使用
} CAN_CHOOSE;

struct canInfo {
	int nFd;
	char *nChanName;
};

// CAN 接收缓冲区
struct CANReceive_Buffer {
	// uint64_t rcv_timestamp;
	uint32_t can_id;
	uint8_t can_dlc;
	uint8_t can_data[8];
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

int can_send(int nFd, uint32_t id, uint8_t len, uint8_t *byteArray);
	
#endif




