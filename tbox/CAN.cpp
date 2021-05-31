
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>
#include <pthread.h>
#include <semaphore.h>
#include <memory>

#include "tbox/Common.h"
#include "tbox/CAN.h"
#include "Mosquitto/MOSQUITTO.h"

#include "DevConfig/ParseXmlCAN.hpp"
#include "DevConfig/ParseConfiguration.h"
#include "DevConfig/PhyChannel.hpp"
#include "ConvertData/ConvertData.h"
#include "CCP/CCP_TEST.h"
#include "DBC/DBC.h"
#include "OBD/OBD.h"

#include "Logger/Logger.h"

static CAN_CHOOSE g_stCanChoose;		//结构体，存储CAN使用情况
static char set_shell[100];	//存储要执行的shell命令

// CAN 消息全局缓冲区
stu_CAN_UOMAP_ChanName_RcvPkg g_stu_CAN_UOMAP_ChanName_RcvPk;

extern Channel_MAP g_Channel_MAP;

typedef enum LETTER_CASE : uint8_t {
	LOWER,
	UPPER
} LETTER_CASE;

// 大小写转换
int strUpLowTrans(char *dst, char *src, LETTER_CASE flag) {
	if (NULL == dst || NULL == src) return -1;

	for (int i = 0; src[i]; i++) {
		if (isalpha(src[i])) {
			dst[i] = (flag == LETTER_CASE::UPPER ? toupper(src[i]) : tolower(src[i]));
		} else {
			dst[i] = src[i];
		}
	}
	return 0;
}

int can_setting(int nChan, int nBuardRate) {
	// 先关闭 can 通道，防止之前未关闭直接设置波特率发生错误
	memset((void *)set_shell, 0, sizeof(set_shell));
	sprintf(set_shell,"ip link set can%c down",(char)(nChan + '0'));
	system(set_shell);

	// set bitrate
	memset((void *)set_shell, 0, sizeof(set_shell));
	sprintf(set_shell,"ip link set can%c type can bitrate %d",(char)(nChan + '0'), nBuardRate);
	system(set_shell);

	// set up
	memset((void *)set_shell, 0, sizeof(set_shell));
	sprintf(set_shell,"ip link set can%c up",(char)(nChan + '0'));
	system(set_shell);

	return 0;
}

int socket_can_init(const char *strNameCAN) {
	int can_fd;
	struct sockaddr_can addr;
    struct ifreq ifr;
	// 结构体变量清零，否则开编译优化时可能出错
	memset(&addr, 0, sizeof(struct sockaddr_can));
	memset(&ifr, 0, sizeof(struct ifreq));

	//socket can
	if ((can_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("socket");
		return -1;
	}

	strncpy(ifr.ifr_name, strNameCAN, strlen(strNameCAN)); 
	if(ioctl(can_fd, SIOCGIFINDEX, &ifr) < 0) {
		perror("SIOCGIFINDEX");
		return -1;
	}

  	addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

   	if(bind(can_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return -1;
	}

	return can_fd;
}

void *can_receive_thread(void *arg) {
	ssize_t nbytes;
	struct can_frame canMessage;
	memset(&canMessage, 0, sizeof(canMessage));

	canInfo *can_info = (struct canInfo *)arg;
	// 保存 socket fd，直接使用可能会出错
	int can_fd = can_info->nFd;
	// 转换通道名称
	char strChannelName[10] = {0};
	strUpLowTrans(strChannelName, can_info->nChanName, LETTER_CASE::UPPER);

	while(1) {
		// Receive packet	
		nbytes = read(can_fd, &canMessage, sizeof(canMessage));
		while(nbytes == -1) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			nbytes = read(can_fd, &canMessage, sizeof(canMessage));	
		}
#if 0
		printf("\n%s(%d) receive   ", strChannelName, can_fd);
		printf("0x%02X    ", canMessage.can_id);
		for (int i = 0; i < canMessage.can_dlc; i++) {
			printf("%02X ", canMessage.data[i]);
		}
		printf("\n");
#endif
		Channel_MAP::iterator ItrChan = g_Channel_MAP.find(strChannelName);
		if (ItrChan == g_Channel_MAP.end()) continue;

		// uint64_t time_stamp = getTimestamp(TIME_STAMP::MS_STAMP);
		CANReceive_Buffer RcvTemp;
		// RcvTemp.rcv_timestamp = time_stamp;
		RcvTemp.can_id = canMessage.can_id;
		RcvTemp.can_dlc = canMessage.can_dlc;
		memcpy(RcvTemp.can_data, canMessage.data, sizeof(canMessage.data));

		do {
			// 检测是否有错误帧
			uint32_t can_err_flags = RcvTemp.can_id & CAN_ERR_FLAG;
			if (can_err_flags) {	// 接收到错误帧
				char temp_can_channel[10] = {0};
				strUpLowTrans(temp_can_channel, strChannelName, LETTER_CASE::LOWER);
				switch (can_err_flags) {
					// TODO: 各种错误检测记录
					case CAN_ERR_BUSOFF:
					case CAN_ERR_BUSERROR:
						memset((void *)set_shell, 0, sizeof(set_shell));
						sprintf(set_shell, "ip link set %s type can restart", temp_can_channel);
						system(set_shell);
						XLOG_WARN("{} restart.", temp_can_channel);
						break;
					case CAN_ERR_RESTARTED:
						XLOG_WARN("{} restart.", temp_can_channel);
						break;
					default: 
						XLOG_WARN("{} received Error frame.", temp_can_channel);
						break;
				}
				break;
			}

			// 存入全局缓冲区
			std::lock_guard<std::mutex> lck(g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer_lock);	
			CAN_UOMAP_ChanName_RcvPkg::iterator Itr = g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer.find(strChannelName);
			if (Itr != g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer.end())  {
				(Itr->second)[canMessage.can_id] = RcvTemp;
			} else {
				CAN_UOMAP_MsgID_Buffer map_buffer;
				map_buffer[RcvTemp.can_id] = RcvTemp;
				g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer[strChannelName] = map_buffer;
			}

		} while(0);
		// Packet for receiving
		memset(&canMessage, 0, sizeof(canMessage));

	}

}

int can_send(int nFd, uint32_t id, uint8_t len, uint8_t *byteArray) {
	if (nFd < 0) return -1;

	ssize_t nbytes;
	struct can_frame canMessage;
	// Prepare packet for write
	memset(&canMessage, 0, sizeof(canMessage));
	canMessage.can_id = id;
	canMessage.can_dlc = len;
	memcpy(canMessage.data, byteArray, sizeof(uint8_t) * len);
	
	nbytes = write(nFd, &canMessage, sizeof(canMessage));
	if (-1 == nbytes) {
		// XLOG_DEBUG("Error when write can message.");
		return -1;
	}
#if 0
	if (nbytes != -1) {
		printf("\ncan_send   ");
        printf("ID: %#X\n", canMessage.can_id);
        for (int i = 0; i < canMessage.can_dlc; i++) {
            printf("%02X ", (canMessage.data[i] & 0xFF));
        }
        printf("\n");
	}
#endif
	return 0;
}

/* Enable/Disables CAN */
int Can_Enable(int isCanEnable) {
	int ReturnCode;
	if ( NO_ERROR != (ReturnCode = DIGIO_Enable_Can(isCanEnable)) )
	{
		// printf("Can-> Module %s ERROR\n", isCanEnable ? "ENABLE":"DISABLE");
		// printf("%s\n",Str_Error(ReturnCode));
		XLOG_DEBUG("Can Module {} ERROR. {}", isCanEnable ? "ENABLE":"DISABLE", Str_Error(ReturnCode));
		return -1;
	}
	// printf("Can-> Module %s is ok\n", isCanEnable ? "ENABLE":"DISABLE");	
	XLOG_DEBUG("Can Module {} is ok.", isCanEnable ? "ENABLE":"DISABLE");
	return 0;
}

int Can_Start(void) {
	pthread_t threadCan[CAN_NUM];	//线程id，每一路CAN对应一个线程

	memset(&g_stCanChoose, 0, sizeof(g_stCanChoose));
	
	printf("-------------------TBox--> Can Module Setting-------------------\n");
	// int nIdxCAN = 0;
	Channel_MAP::iterator ItrChan = g_Channel_MAP.begin();
	for (; ItrChan != g_Channel_MAP.end(); ItrChan++) {
		if (ItrChan->first == "CAN1") {
			// 记录使用情况
			g_stCanChoose.wCan[0] = 1;
			can_setting(1, ItrChan->second.CAN.stuBuardRate.nCANBR);
			int fd = socket_can_init("can1");
			if (fd < 0) {
				XLOG_CRITICAL("can1 socket_init ERROR.");
				return -1;
			}
			
			struct canInfo *info = (struct canInfo *)malloc(sizeof(struct canInfo));
			info->nChanName = (char *)"CAN1";
			info->nFd = fd;

			while (getTimestamp(TIME_STAMP::MS_STAMP) % 1000 > 10);
			while(0 != pthread_create(&threadCan[0], NULL, can_receive_thread, (void *)info) ) {
				perror("can1 pthread_create");
			}
			pthread_setname_np(threadCan[0], "can1 receive");
			pthread_detach(threadCan[0]);

			// TODO
			// DBC
			if (!ItrChan->second.CAN.mapDBC.empty()) {
				// std::shared_ptr<DBC> objDBC = std::make_shared<DBC>(info);
				DBC *objDBC = new DBC(std::move(*info));
				if(0 == objDBC->Init()) {

				}

			}
			// CCP
			int nNumCCP = ItrChan->second.CAN.mapCCP.size();
			if (nNumCCP) {
				std::vector<CCP_TEST*> vecCCP(nNumCCP);
				CCP_MAP_Salve_DAQList::iterator Itr = ItrChan->second.CAN.mapCCP.begin();
				for (int i = 0; i < nNumCCP; i++) {
					vecCCP[i] = new CCP_TEST(*info, Itr->first);
					if (0 == vecCCP[i]->Init()) {

					}
					Itr++;
				}
			}

		} else if (ItrChan->first == "CAN2") {
			g_stCanChoose.wCan[1] = 1;
			can_setting(2, ItrChan->second.CAN.stuBuardRate.nCANBR);
			int fd = socket_can_init("can2");
			if (fd < 0) {
				XLOG_CRITICAL("can2 socket_init");
				return -2;
			}

			// struct canInfo info((canInfo){fd, (char *)"CAN2"});
			struct canInfo *info = (struct canInfo *)malloc(sizeof(struct canInfo));
			info->nChanName = (char *)"CAN2";
			info->nFd = fd;

			while(0 != pthread_create(&threadCan[1], NULL, can_receive_thread, (void *)info) ) {
				perror("can2 pthread_create");
			}
			pthread_setname_np(threadCan[1], "can2 receive");
			pthread_detach(threadCan[1]);

			if (!ItrChan->second.CAN.mapDBC.empty()) {
				// std::shared_ptr<DBC> objDBC = std::make_shared<DBC>(info);
				DBC *objDBC = new DBC(std::move(*info));
				if(0 == objDBC->Init()) {

				}

			}

			// TODO
			int nNumCCP = ItrChan->second.CAN.mapCCP.size();
			if (nNumCCP) {
				std::vector<CCP_TEST*> vecCCP(nNumCCP);
				CCP_MAP_Salve_DAQList::iterator Itr = ItrChan->second.CAN.mapCCP.begin();
				for (int i = 0; i < nNumCCP; i++) {
					vecCCP[i] = new CCP_TEST(*info, Itr->first);
					
					if (0 == vecCCP[i]->Init()) {
						// int ret = vecCCP[i]->DAQList_Initialization();
    					// if (-1 == ret) {
        				// 	XLOG_INFO("CAN2 DAQList_Initialization ERROR.");
    					// }
					}
					Itr++;
				}
			}

			if (!ItrChan->second.CAN.mapOBD.empty()) {
				OBD *objOBD = new OBD(std::move(*info));
				if(0 == objOBD->Init()) {

				}

			}

		}
	}
		
	printf("---------------TBox--> Can Module Setting Finish!---------------\n");
	XLOG_INFO("Can Module initialized & started finished.");
	return 0;
}

void Can_Stop(void) {
//	int ret = 0;
	for(int i = 0; i < CAN_NUM; i++) {
//		printf("wCan = %d,canChoose.wCan[wCan] = %d,%d\n",wCan,canChoose.wCan[wCan],CAN_NUM);
		if(1 == g_stCanChoose.wCan[i]) {
			memset((void *)set_shell, 0, sizeof(set_shell));
			sprintf(set_shell,"ip link set can%c down", (char)(i + 1 + 48));
			// printf("stopping can%c...\n",(char)(i + 1 + 48));
			if(0 != system(set_shell) ) {
				continue;
			}
//			printf("%d\n",ret);
		}
	}
	Can_Enable(DISABLE_CAN);
}
