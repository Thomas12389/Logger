
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/can.h>
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
#include "XCP/XCP_TEST.h"
#include "DBC/DBC.h"
#include "OBD/OBD.h"

#include "Logger/Logger.h"

#include "tfm_common.h"
#include "BLF_LOG/BlfCan.h"


static CAN_CHOOSE g_stCanChoose;		//结构体，存储CAN使用情况
static char set_shell[100];	//存储要执行的shell命令
static std::mutex g_can_wlock[4];		// 向 CAN 写入数据的锁

BlfCan g_BlfCan;

extern int OBD_Run;
extern int CCP_Run;
extern int XCP_Run;

// CAN 各协议 ID
mapID_Register g_mapID_Register;
// CAN 消息全局缓冲区
stu_CAN_UOMAP_ChanName_RcvPkg g_stu_CAN_UOMAP_ChanName_RcvPk;

extern Channel_MAP g_Channel_MAP;

int can_setting(int nChan, int nBuardRate, int nDBuardRate) {
	// 先关闭 can 通道，防止之前未关闭直接设置波特率发生错误
	memset((void *)set_shell, 0, sizeof(set_shell));
	sprintf(set_shell,"ip link set can%c down",(char)(nChan + '0'));
	system(set_shell);

	// set bitrate
	memset((void *)set_shell, 0, sizeof(set_shell));
	sprintf(set_shell,"ip link set can%c type can bitrate %d", (char)(nChan + '0'), nBuardRate);
	if (nDBuardRate != 0) {
		char fd_dbitrate[50] = {0};
		sprintf(fd_dbitrate, " dbitrate %d fd on", nDBuardRate);
		strcat(set_shell, fd_dbitrate);
	}

	system(set_shell);

	memset((void *)set_shell, 0, sizeof(set_shell));
	sprintf(set_shell, "ip link set can%c type can restart-ms 10", (char)(nChan + '0'));
	system(set_shell);

	// set up
	memset((void *)set_shell, 0, sizeof(set_shell));
	sprintf(set_shell,"ip link set can%c up",(char)(nChan + '0'));
	system(set_shell);

	return 0;
}

bool can_rcv_filters_set(const int canFd, const std::vector<uint32_t> canIdArray, const uint32_t arraySize, const uint8_t filterType) {

	if (canFd < 0) return false;
	// 不需要接收任何报文
	if (0 == arraySize) {
		if (-1 == setsockopt(canFd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0) ) {
			XLOG_ERROR("Set can filter(send only) ERROR, {}.", strerror(errno));
			return false;
		}
		return true;
	}

	struct can_filter rfilter[arraySize];
	bzero(rfilter, sizeof(rfilter));

	for (uint32_t i = 0; i < arraySize; i++) {
		if (canIdArray[i]) {
			rfilter[i].can_id = canIdArray[i];
			rfilter[i].can_mask = (canIdArray[i] & CAN_EFF_FLAG) ? CAN_EFF_MASK : CAN_SFF_MASK;
		} else {
			return false;
		}
	}
	if (filterType & CAN_FILTER_REJECT) {

		int join_filter = 1;
		// 5.3 工具链中没有 CAN_RAW_JOIN_FILTERS 枚举，可自行添加
		// 文件 /opt/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/arm-linux-gnueabihf/libc/usr/include/linux/can/raw.h
		if (-1 == setsockopt(canFd, SOL_CAN_RAW, 6/* CAN_RAW_JOIN_FILTERS */, &join_filter, sizeof(join_filter)) ) {
			XLOG_ERROR("Set can filter(join) ERROR, {}.", strerror(errno));
			return false;
		}
	}
	
	if (-1 == setsockopt(canFd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) ) {
		XLOG_ERROR("Set can filter ERROR, {}.", strerror(errno));
		return false;
	}
	return true;
}

// can 初始化， 全小写
int socket_can_init(const char *strNameCAN) {
	int can_fd;
	int ret = 0;
	struct sockaddr_can addr;
	struct ifreq ifr;
	// 结构体变量清零，否则开编译优化时可能出错
	memset(&addr, 0, sizeof(struct sockaddr_can));
	memset(&ifr, 0, sizeof(struct ifreq));

	//socket can
	int protocol = CAN_RAW;
	if ((can_fd = socket(PF_CAN, SOCK_RAW, protocol)) < 0) {
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

	char strTempName[10] = {0};
	strUpLowTrans(strTempName, strNameCAN, LETTER_CASE::UPPER);

	// 查找通道
	Channel_MAP::iterator ItrChan = g_Channel_MAP.find(strTempName);
	if (ItrChan == g_Channel_MAP.end()) return -1;

	// 2022.06.20
	if (false == (ItrChan->second).CAN.enableTraffic) {
		// 根据配置设置过滤器
		std::vector<uint32_t> can_id_vec;

		/* 查找所有需要接收的 ID */
		// DBC 需要接收的 ID
		COM_MAP_MsgID_RMsgPkg::iterator ItrDBC = (ItrChan->second).CAN.mapDBC.begin();
		for (; ItrDBC != (ItrChan->second).CAN.mapDBC.end(); ItrDBC++) {
			// 若已经存在该 ID，则不再加入
			if (can_id_vec.end() != std::find(can_id_vec.begin(), can_id_vec.end(), ItrDBC->first)) continue;
			can_id_vec.emplace_back(ItrDBC->first);
		}
		// CCP 需要接收的 ID
		CCP_MAP_Slave_DAQList::iterator ItrCCP = (ItrChan->second).CAN.mapCCP.begin();
		for (; ItrCCP != (ItrChan->second).CAN.mapCCP.end(); ItrCCP++) {
			// DTO ID
			can_id_vec.emplace_back((ItrCCP->first).nIdDTO);
			std::vector<CCP_DAQList>::iterator ItrVecDAQList = (ItrCCP->second).begin();
			for (; ItrVecDAQList != (ItrCCP->second).end(); ItrVecDAQList++) {
				// 若已经存在该 ID，则不再加入
				if (can_id_vec.end() != std::find(can_id_vec.begin(), can_id_vec.end(), (*ItrVecDAQList).nCANID)) continue;
				can_id_vec.emplace_back((*ItrVecDAQList).nCANID);
			}
		}
		// XCP 需要接收的 ID
		XCP_MAP_Slave_DAQList::iterator ItrXCP = (ItrChan->second).CAN.mapXCP.begin();
		for (; ItrXCP != (ItrChan->second).CAN.mapXCP.end(); ItrXCP++) {
			// Slave ID
			can_id_vec.emplace_back((ItrXCP->first).nSlaveID);
			std::vector<XCP_DAQList>::iterator ItrVecDAQList = (ItrXCP->second).begin();
			for (; ItrVecDAQList != (ItrXCP->second).end(); ItrVecDAQList++) {
				if (can_id_vec.end() != std::find(can_id_vec.begin(), can_id_vec.end(), (*ItrVecDAQList).nCANID)) continue;
				can_id_vec.emplace_back((*ItrVecDAQList).nCANID);
			}
		}
		// OBD 需要接收的 ID
		OBD_MAP_MsgID_Mode01_RMsgPkg::iterator ItrOBD = (ItrChan->second).CAN.OBDII.mapOBD.begin();
		for (; ItrOBD != (ItrChan->second).CAN.OBDII.mapOBD.end(); ItrOBD++) {

			std::map<uint32_t, OBD_Mode01RecvPackage*>::iterator ItrRSP = (ItrOBD->second).begin();
			for (; ItrRSP != (ItrOBD->second).end(); ItrRSP++) {
				if ( can_id_vec.end() != std::find(can_id_vec.begin(), can_id_vec.end(), ItrRSP->first) ) continue;
				can_id_vec.emplace_back(ItrRSP->first);
			}
		}

		// 使能硬件过滤
		if ((ItrChan->second).CAN.enableHWFilter) {
			filter_data AllFilterValues[MAX_HWFILTERS_CAN3_CAN4];
			memset(AllFilterValues, 0x00, sizeof(AllFilterValues));
			struct set_can_filter HWFilterCan = {0, AllFilterValues};

			uint32_t max_filters = MAX_HWFILTERS_CAN1_CAN2;
			if (!strcmp("CAN3", strTempName) || !strcmp("CAN4", strTempName)) {
				max_filters = MAX_HWFILTERS_CAN3_CAN4;
			}

			uint32_t filter_size = can_id_vec.size() /* < max_filters ? can_id_vec.size() : max_filters */;
			if (can_id_vec.size() > max_filters) {
				filter_size = max_filters;
				XLOG_WARN("{} filters to be is {} more than max filters({}).", strTempName, can_id_vec.size() - max_filters, max_filters);
			}
			for (uint32_t i = 0; i < filter_size; i++) {
				if (can_id_vec[i]) {
					AllFilterValues[i].filter = can_id_vec[i];
					AllFilterValues[i].flags |= ( (can_id_vec[i] & CAN_EFF_FLAG) ? RX_EXTENDED_ID : RX_STANDARD_ID );
					AllFilterValues[i].mask = (can_id_vec[i] & CAN_EFF_FLAG) ? CAN_EFF_MASK : CAN_SFF_MASK;

					HWFilterCan.count++;
				}
			}

			ifr.ifr_ifru.ifru_data = (char *)&HWFilterCan;
			if (ioctl(can_fd, SIOCSETFILTER, &ifr) < 0) {
				XLOG_ERROR("Set HWfilter({}) ERROR, {}.", strerror(errno));
			} else {
				XLOG_INFO("Set HWfilter({}) successfully, size = {}.", strTempName, filter_size);
			}
		} else {
			// 传输层过滤，软件
			// 5.3 工具链中没有 CAN_RAW_FILTER_MAX 宏，可自行添加
			// 文件 /opt/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/arm-linux-gnueabihf/libc/usr/include/linux/can.h
			uint32_t filter_size = can_id_vec.size() < 512/* CAN_RAW_FILTER_MAX */ ? can_id_vec.size() : 512/* CAN_RAW_FILTER_MAX */;
			if (can_rcv_filters_set(can_fd, can_id_vec, filter_size, CAN_FILTER_PASS) ) {
				XLOG_INFO("Set filter({}) successfully, size = {}.", strTempName, filter_size);
			} else {
				XLOG_ERROR("Set filter({}) ERROE.", strTempName);
			}
		}
	}

	// 关闭本地回环
	int lookback = 0;
	setsockopt(can_fd, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &lookback, sizeof(lookback));
	// CAN3、CAN4 支持 CANFD
	if (!strcmp("CAN3", strTempName) || !strcmp("CAN4", strTempName)) {
		int fd_on = 1;
		setsockopt(can_fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &fd_on, sizeof(fd_on));
	}

	if(bind(can_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		return -1;
	}

	return can_fd;
}

void *can_receive_thread(void *arg) {
	ssize_t nbytes;
	struct canfd_frame canMessage;
	memset(&canMessage, 0, sizeof(canMessage));

	canInfo *can_info = (struct canInfo *)arg;
	// 保存 socket fd，直接使用可能会出错
	int can_fd = can_info->nFd;
	// 转换通道名称
	char strChannelName[10] = {0};
	strUpLowTrans(strChannelName, can_info->nChanName, LETTER_CASE::UPPER);

	// 记录 ID 与 FIFO 的对应关系
	std::map<uint32_t, std::vector<int> > mapID_WFD;
	mapID_Register::iterator ItrReg = g_mapID_Register.find(strChannelName);
	if (ItrReg == g_mapID_Register.end()) {
		XLOG_WARN("{} no messages need to receive, receive thread exited.", strChannelName);
		pthread_exit(NULL);
	}
	// 根据注册信息提取 ID 与 fifo 写端 fd 的对应关系
	mapProtype_IDs::iterator ItrPro = ItrReg->second.begin();
	for (; ItrPro != ItrReg->second.end(); ItrPro++) {
		std::vector<RegInfo>::iterator Itrvec = ItrPro->second.begin();
		// 保存 ID 与写端 fd 对应关系
		for (; Itrvec != ItrPro->second.end(); Itrvec++) {
			// 打开对应 FIFO
			int w_fd = open((*Itrvec).fifo_path.c_str(), O_WRONLY);
			if (w_fd < 0) {
				XLOG_ERROR("ERROR {} with {}, {}", strChannelName, protocol_enum_to_string(ItrPro->first), strerror(errno));
				continue;
			}

			std::vector<uint32_t>::iterator ItrId = (*Itrvec).reg_ids.begin();
			for (; ItrId != (*Itrvec).reg_ids.end(); ItrId++) {
				mapID_WFD[*ItrId].push_back(w_fd);
			}
		}
	}

#if 0
	std::map<uint32_t, std::vector<int> >::iterator Itr = mapID_WFD.begin();
	// 打印该通道 ID 与 w_fd 的对应情况
    for (; Itr != mapID_WFD.end(); Itr++) {
        printf("channelname: %s\n", strChannelName);
		printf("\tID: %#X  ", Itr->first);
        std::vector<int>::iterator Itrvec = Itr->second.begin();
        printf("\twrite fd:");
        for (; Itrvec != Itr->second.end(); Itrvec++) {
            printf("%d  ", *Itrvec);
        }
        printf("\n");
    }
	Itr = mapID_WFD.begin();
#endif

	int chanNo = 0;
	sscanf(strChannelName, "CAN%d", &chanNo);
	while (1) {
		// Receive packet	
		nbytes = read(can_fd, &canMessage, CANFD_MTU);
		// printf("nbytes = %" PRId32 ", time: %" PRIu64 "\n", nbytes, getTimestamp(TIME_STAMP::MS_STAMP));
		if (nbytes != CAN_MTU && nbytes != CANFD_MTU) continue;

		// 该通道 traffic 使能，且 blf 记录状态可用
		if (g_Channel_MAP[strChannelName].CAN.enableTraffic && g_BlfCan.getBlfCanAvaliable()) {
			// 获取时间戳
			struct timeval t_ns;
			memset(&t_ns, 0, sizeof(timeval));
			if (ioctl(can_fd, SIOCGSTAMP, &t_ns) < 0) {
				XLOG_ERROR("Get {} message timestamp ERROR.", strChannelName);
			}
			// 存报文
			if (nbytes == CAN_MTU) {
				g_BlfCan.writeCanMessage(chanNo, CAN_DIR_RX, ((int64_t)t_ns.tv_sec * 1000000 + t_ns.tv_usec) * 1000, *((struct can_frame *)&canMessage));
			} else if (nbytes == CANFD_MTU) {
				g_BlfCan.writeCanFdMessage(chanNo, CAN_DIR_RX, ((int64_t)t_ns.tv_sec * 1000000 + t_ns.tv_usec) * 1000, canMessage);
			}
		}

		mapID_Register::iterator ItrChan = g_mapID_Register.find(strChannelName);
		if (ItrChan == g_mapID_Register.end()) continue;

		// 查找 ID
		std::map<uint32_t, std::vector<int> >::iterator Itr = mapID_WFD.find(canMessage.can_id);
		if (Itr == mapID_WFD.end()) {
			// XLOG_WARN("{} ID: {} does not need.", strChannelName, canMessage.can_id);
			continue;
		}

		CANReceive_Buffer RcvTemp;
		RcvTemp.can_id = canMessage.can_id;
		RcvTemp.can_dlc = canMessage.len;
		memcpy(RcvTemp.can_data, canMessage.data, sizeof(canMessage.data));
		
		// 根据注册情况路由到对应的 fifo
		std::vector<int>::iterator Itrvec = Itr->second.begin();
		for (; Itrvec != Itr->second.end(); Itrvec++) {
			write((*Itrvec), &RcvTemp, sizeof(RcvTemp));
		}

	}

}

int can_send(std::string chanName, int nFd, uint32_t id, uint8_t len, uint8_t *byteArray) {
	if (nFd < 0) return -1;
	if (NULL == byteArray) return -1;

	ssize_t nbytes;
	int chanNo = 0;
	sscanf(chanName.c_str(), "CAN%d", &chanNo);
	
	// 获取写入锁
	std::lock_guard<std::mutex> lock(g_can_wlock[chanNo - 1]);
	// 普通 CAN
	bool canfd_flag = false;
	if (!canfd_flag) {
		struct can_frame canMessage;
		memset(&canMessage, 0, sizeof(canMessage));
		canMessage.can_id = id;
		canMessage.can_dlc = (len > 8) ? 8 : len;
		memcpy(canMessage.data, byteArray, sizeof(uint8_t) * canMessage.can_dlc);
		
		nbytes = write(nFd, &canMessage, sizeof(canMessage));
		if (-1 == nbytes) {
			XLOG_DEBUG("Error when write can message.");
			return -1;
		}
		// traffic 可用
		if (g_Channel_MAP[chanName].CAN.enableTraffic && g_BlfCan.getBlfCanAvaliable()) {
			g_BlfCan.writeCanMessage(chanNo, CAN_DIR_TX, 0, canMessage);
		}
	} else {
		struct canfd_frame canFdMessage;
		// Prepare packet for write
		memset(&canFdMessage, 0, sizeof(canFdMessage));
		canFdMessage.can_id = id;
		canFdMessage.len = len;
		memcpy(canFdMessage.data, byteArray, sizeof(uint8_t) * len);
		
		nbytes = write(nFd, &canFdMessage, sizeof(canFdMessage));
		if (-1 == nbytes) {
			XLOG_DEBUG("Error when write canfd message.");
			return -1;
		}
		if (g_Channel_MAP[chanName].CAN.enableTraffic && g_BlfCan.getBlfCanAvaliable()) {
			g_BlfCan.writeCanFdMessage(chanNo, CAN_DIR_TX, 0, canFdMessage);
		}
	}
#if 0
	if (nbytes != -1) {
		printf("\ncan_send   ");
        printf("ID: %#X\n", canMessage.can_id);
        for (int i = 0; i < canMessage.len; i++) {
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
	if ( NO_ERROR != (ReturnCode = DIGIO_Enable_Can(isCanEnable)) ) {
		// printf("Can-> Module %s ERROR\n", isCanEnable ? "ENABLE":"DISABLE");
		// printf("%s\n",Str_Error(ReturnCode));
		XLOG_DEBUG("Can Module {} ERROR. {}", isCanEnable ? "ENABLE":"DISABLE", Str_Error(ReturnCode));
		return -1;
	}
	// printf("Can-> Module %s is ok\n", isCanEnable ? "ENABLE":"DISABLE");	
	XLOG_DEBUG("Can Module {} is ok.", isCanEnable ? "ENABLE":"DISABLE");
	return 0;
}

int Can_Start(const char *blfName) {

	// 查询是否有通道使能了 traffic 功能 -- 2022.06.20
	int enableTraffic = CAN_TRAFFIC_OFF;
	Channel_MAP::iterator ItrChan = g_Channel_MAP.begin();
	for (; ItrChan != g_Channel_MAP.end(); ItrChan++) {
		if (ItrChan->second.CAN.enableTraffic) {
			enableTraffic = CAN_TRAFFIC_ON;
			break;
		}
	}
	if (CAN_TRAFFIC_ON == enableTraffic) {
		if (NULL == blfName) {
			XLOG_ERROR("Can blf log filename error!");
		} else {
			std::string name(blfName);
			std::string traffic_path = CAN_TRAFFIC_DIR + name;

			if (-1 == access(CAN_TRAFFIC_DIR.c_str(), F_OK) ) {
				if (-1 == mkdir(CAN_TRAFFIC_DIR.c_str(), 0775)) {
					XLOG_ERROR("make can traffic dir: {}.", strerror(errno));
				}
			}
			if (g_BlfCan.start(traffic_path)) {
				g_BlfCan.setDefaultMaxObject(0x140000);
				XLOG_INFO("Can blf log start.");
			} else {
				XLOG_ERROR("Can blf log start error!");
			}
		}

	}
	
	pthread_t threadCan[CAN_NUM];	//线程id，每一路CAN对应一个线程

	memset(&g_stCanChoose, 0, sizeof(g_stCanChoose));
	
	// printf("-------------------TBox--> Can Module Setting-------------------\n");
	// int nIdxCAN = 0;
	ItrChan = g_Channel_MAP.begin();
	for (; ItrChan != g_Channel_MAP.end(); ItrChan++) {
		if (ItrChan->first == "CAN1") {
			// 记录使用情况
			g_stCanChoose.wCan[0] = 1;
			can_setting(1, ItrChan->second.CAN.stuBuardRate.nCANBR, 0);
			int fd = socket_can_init("can1");
			if (fd < 0) {
				XLOG_CRITICAL("can1 socket_init ERROR.");
				continue;
			}
			
			struct canInfo *info = (struct canInfo *)malloc(sizeof(struct canInfo));
			info->nChanName = (char *)"CAN1";
			info->nFd = fd;

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
				CCP_MAP_Slave_DAQList::iterator Itr = ItrChan->second.CAN.mapCCP.begin();
				for (int i = 0; i < nNumCCP; i++) {
					vecCCP[i] = new CCP_TEST(*info, Itr->first);
					if (0 == vecCCP[i]->Init()) {

					}
					Itr++;
				}
			}
			// XCP
			int nNumXCP = ItrChan->second.CAN.mapXCP.size();
			if (nNumXCP) {
				std::vector<XCP_TEST*> vecXCP(nNumXCP);
				XCP_MAP_Slave_DAQList::iterator Itr = ItrChan->second.CAN.mapXCP.begin();
				for (int i = 0; i < nNumXCP; i++) {
					vecXCP[i] = new XCP_TEST(*info, Itr->first);
					if (0 == vecXCP[i]->Init()) {

					}
					Itr++;
				}
			}
			// OBD
			if (!(ItrChan->second).CAN.OBDII.mapOBD.empty()) {
				OBD *objOBD = new OBD(*info, (ItrChan->second).CAN.OBDII.enableSinglePidRequest);
				if (0 == objOBD->Init()) {
					
				}
			}

			while (getTimestamp(TIME_STAMP::MS_STAMP) % 1000 > 10);
			while(0 != pthread_create(&threadCan[0], NULL, can_receive_thread, (void *)info) ) {
				perror("can1 pthread_create");
			}
			pthread_setname_np(threadCan[0], "can1 receive");
			pthread_detach(threadCan[0]);

		} else if (ItrChan->first == "CAN2") {
			// 记录使用情况
			g_stCanChoose.wCan[1] = 1;
			can_setting(2, ItrChan->second.CAN.stuBuardRate.nCANBR, 0);
			int fd = socket_can_init("can2");
			if (fd < 0) {
				XLOG_CRITICAL("can2 socket_init ERROR.");
				continue;
			}
			
			struct canInfo *info = (struct canInfo *)malloc(sizeof(struct canInfo));
			info->nChanName = (char *)"CAN2";
			info->nFd = fd;

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
				CCP_MAP_Slave_DAQList::iterator Itr = ItrChan->second.CAN.mapCCP.begin();
				for (int i = 0; i < nNumCCP; i++) {
					vecCCP[i] = new CCP_TEST(*info, Itr->first);
					if (0 == vecCCP[i]->Init()) {

					}
					Itr++;
				}
			}
			// XCP
			int nNumXCP = ItrChan->second.CAN.mapXCP.size();
			if (nNumXCP) {
				std::vector<XCP_TEST*> vecXCP(nNumXCP);
				XCP_MAP_Slave_DAQList::iterator Itr = ItrChan->second.CAN.mapXCP.begin();
				for (int i = 0; i < nNumXCP; i++) {
					vecXCP[i] = new XCP_TEST(*info, Itr->first);
					if (0 == vecXCP[i]->Init()) {

					}
					Itr++;
				}
			}
			// OBD
			if (!(ItrChan->second).CAN.OBDII.mapOBD.empty()) {
				OBD *objOBD = new OBD(*info, (ItrChan->second).CAN.OBDII.enableSinglePidRequest);
				if (0 == objOBD->Init()) {
					
				}
			}

			while (getTimestamp(TIME_STAMP::MS_STAMP) % 1000 > 10);
			while(0 != pthread_create(&threadCan[1], NULL, can_receive_thread, (void *)info) ) {
				perror("can2 pthread_create");
			}
			pthread_setname_np(threadCan[1], "can2 receive");
			pthread_detach(threadCan[1]);

		} 
		else if (ItrChan->first == "CAN3") {
			// 记录使用情况
			g_stCanChoose.wCan[2] = 1;
			can_setting(3, ItrChan->second.CAN.stuBuardRate.nCANBR, ItrChan->second.CAN.stuBuardRate.nCANFDBR);
			int fd = socket_can_init("can3");
			if (fd < 0) {
				XLOG_CRITICAL("can3 socket_init ERROR.");
				continue;
			}
			
			struct canInfo *info = (struct canInfo *)malloc(sizeof(struct canInfo));
			info->nChanName = (char *)"CAN3";
			info->nFd = fd;

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
				CCP_MAP_Slave_DAQList::iterator Itr = ItrChan->second.CAN.mapCCP.begin();
				for (int i = 0; i < nNumCCP; i++) {
					vecCCP[i] = new CCP_TEST(*info, Itr->first);
					if (0 == vecCCP[i]->Init()) {

					}
					Itr++;
				}
			}
			// XCP
			int nNumXCP = ItrChan->second.CAN.mapXCP.size();
			if (nNumXCP) {
				std::vector<XCP_TEST*> vecXCP(nNumXCP);
				XCP_MAP_Slave_DAQList::iterator Itr = ItrChan->second.CAN.mapXCP.begin();
				for (int i = 0; i < nNumXCP; i++) {
					vecXCP[i] = new XCP_TEST(*info, Itr->first);
					if (0 == vecXCP[i]->Init()) {

					}
					Itr++;
				}
			}
			// OBD
			if (!(ItrChan->second).CAN.OBDII.mapOBD.empty()) {
				OBD *objOBD = new OBD(*info, (ItrChan->second).CAN.OBDII.enableSinglePidRequest);
				if (0 == objOBD->Init()) {
					
				}
			}

			while (getTimestamp(TIME_STAMP::MS_STAMP) % 1000 > 10);
			while(0 != pthread_create(&threadCan[2], NULL, can_receive_thread, (void *)info) ) {
				perror("can3 pthread_create");
			}
			pthread_setname_np(threadCan[2], "can3 receive");
			pthread_detach(threadCan[2]);

		} else if (ItrChan->first == "CAN4") {
			g_stCanChoose.wCan[3] = 1;
			can_setting(4, ItrChan->second.CAN.stuBuardRate.nCANBR, ItrChan->second.CAN.stuBuardRate.nCANFDBR);
			int fd = socket_can_init("can4");
			if (fd < 0) {
				XLOG_CRITICAL("can4 socket_init");
				continue;
			}

			// struct canInfo info((canInfo){fd, (char *)"CAN2"});
			struct canInfo *info = (struct canInfo *)malloc(sizeof(struct canInfo));
			info->nChanName = (char *)"CAN4";
			info->nFd = fd;

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
				CCP_MAP_Slave_DAQList::iterator Itr = ItrChan->second.CAN.mapCCP.begin();
				for (int i = 0; i < nNumCCP; i++) {
					vecCCP[i] = new CCP_TEST(*info, Itr->first);
					
					if (0 == vecCCP[i]->Init()) {

					}
					Itr++;
				}
			}

			// XCP
			int nNumXCP = ItrChan->second.CAN.mapXCP.size();
			if (nNumXCP) {
				std::vector<XCP_TEST*> vecXCP(nNumXCP);
				XCP_MAP_Slave_DAQList::iterator Itr = ItrChan->second.CAN.mapXCP.begin();
				for (int i = 0; i < nNumXCP; i++) {
					vecXCP[i] = new XCP_TEST(*info, Itr->first);
					if (0 == vecXCP[i]->Init()) {

					}
					Itr++;
				}
			}
			// OBD
			if (!(ItrChan->second).CAN.OBDII.mapOBD.empty()) {
				OBD *objOBD = new OBD(*info, (ItrChan->second).CAN.OBDII.enableSinglePidRequest);
				if (0 == objOBD->Init()) {
					
				}
			}

			while (getTimestamp(TIME_STAMP::MS_STAMP) % 1000 > 10);
			while(0 != pthread_create(&threadCan[3], NULL, can_receive_thread, (void *)info) ) {
				perror("can4 pthread_create");
			}
			pthread_setname_np(threadCan[3], "can4 receive");
			pthread_detach(threadCan[3]);

		}
	}

#if 0
	// 打印通道的注册情况
	mapID_Register::iterator Itr = g_mapID_Register.begin();
	for (; Itr != g_mapID_Register.end(); Itr++) {
		printf("channelname: %s\n", Itr->first.c_str());
		for (auto Itr2 = Itr->second.begin(); Itr2 != Itr->second.end(); Itr2++) {
			printf("\tpro_type: %d\n", Itr2->first);
			printf("\t\t");
			for (auto Itr3 = Itr2->second.begin(); Itr3 != Itr2->second.end(); Itr3++) {
				printf("fifo_path: %s\t", (*Itr3).fifo_path.c_str());
				for (auto ItrID = (*Itr3).reg_ids.begin(); ItrID != (*Itr3).reg_ids.end(); ItrID++) {
					printf("%#X ", *ItrID);
				}
			}
			printf("\n");
		}
	}
	printf("\n");

	printf("---------------TBox--> Can Module Setting Finish!---------------\n");
#endif
		
	XLOG_INFO("Can Module initialized & started finished.");
	return 0;
}

void Can_Stop(void) {
	// 停止协议线程
	OBD_Run = 0;
	CCP_Run = 0;
	XCP_Run = 0;

//	int ret = 0;
	for(int i = 0; i < CAN_NUM; i++) {
//		printf("wCan = %d,canChoose.wCan[wCan] = %d,%d\n",wCan,canChoose.wCan[wCan],CAN_NUM);
		if(1 == g_stCanChoose.wCan[i]) {
			memset((void *)set_shell, 0, sizeof(set_shell));
			sprintf(set_shell,"ip link set can%c down", (char)(i + 1 + 48));
			// printf("stopping can%c...\n",(char)(i + 1 + 48));
			if(0 != system(set_shell) ) {
				// XLOG_ERROR("Stop CAN{0:d} ERROR.", (i + 1));
				continue;
			}
//			printf("%d\n",ret);
			// XLOG_INFO("Stop CAN{0:d} ok.", (i + 1));
			// printf("stop can%c ok\n",(char)(i + 1 + 48));
		}
	}
	Can_Enable(DISABLE_CAN);

	if (g_BlfCan.getBlfCanAvaliable()) {
		g_BlfCan.stop();
	}
}


int can_register(std::string chn_name, protocol_type type, RegInfo reginfo) {

    mapID_Register::iterator Itr = g_mapID_Register.find(chn_name);
	if (Itr == g_mapID_Register.end()) {
		mapProtype_IDs temp;
		temp[type].push_back(reginfo);
		g_mapID_Register[chn_name] = temp;
	} else {
		Itr->second[type].push_back(reginfo);
	}

	return 0;
}

const char* protocol_enum_to_string(protocol_type type) {
	switch (type) {
		case protocol_type::PRO_DBC: {
			return "DBC";
		}
		case protocol_type::PRO_CCP: {
			return "CCP";
		}
		case protocol_type::PRO_XCP: {
			return "XCP";
		}
		case protocol_type::PRO_OBD: {
			return "OBD";
		}
		case protocol_type::PRO_WWHOBD: {
			return "WWHOBD";
		}
		case protocol_type::PRO_J1939: {
			return "J1939";
		}
		case protocol_type::PRO_UDS: {/*  */
			return "UDS";
		}
	
		default:
			return "";
	}
}