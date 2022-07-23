
#ifndef __PHYCHANNEL_H__
#define __PHYCHANNEL_H__

#include "DevConfig/ParseXmlCAN.hpp"
#include "DevConfig/ParseXmlCAN_DBC.hpp"
#include "DevConfig/ParseXmlCAN_CCP.hpp"
#include "DevConfig/ParseXmlCAN_XCP.hpp"
#include "DevConfig/ParseXmlCAN_OBD.hpp"
#include "DevConfig/ParseXmlInside.hpp"

struct Channel_Info {
	struct CAN{
		bool enableHWFilter;	// 2022.03.22
		bool enableTraffic;		// 2022.06.20
		CAN_BuardRate stuBuardRate;
		COM_MAP_MsgID_RMsgPkg mapDBC;
		CCP_MAP_Slave_DAQList mapCCP;
		XCP_MAP_Slave_DAQList mapXCP;
		// 2022.07.07
		struct {
			bool enableSinglePidRequest;
			OBD_MAP_MsgID_Mode01_RMsgPkg mapOBD;
		} OBDII;
	} CAN;
};

// Channel 
typedef std::map<std::string, Channel_Info> Channel_MAP;		// string -- CAN1/CAN2/CAN3/CAN4

struct Inside_Message{
	uint8_t isEnable;
	int nNumSigs;
	Inside_SigStruct* pInsideSig;
	uint32_t nSampleCycleMs;
};

typedef std::map<std::string, Inside_Message> Inside_MAP;		// string -- GPS or Internal

// device 
struct Hardware_Info {
	std::string SeqNum;
};

// File
struct File_Save {
	bool bIsActive;
	std::string strLocalDirName;		//	本地保存临时数据文件的文件夹，以启动时的本地时间命名
	std::string strRemoteDirName;		//	上传至 FTP 服务器时，文件存放的远端目录，该目录以 Hardware_Info 中的 SeqNum 命名
	std::string strConfigFileName;		//	配置文件中的文件名
	uint32_t nSaveCycleMs;
};

// Net
struct FTP_Server {
	bool bIsActive;
	bool bIsSFTP;
	bool bDeleteAfterUpload;
    std::string strIP;
    uint16_t nPort;
    std::string strUserName;
    std::string strPasswd;
};

struct MQTT_Server {
	bool bIsActive;
	bool bIsTLSEnabled;
    std::string strIP;
    uint16_t nPort;
    std::string strUserName;
    std::string strPasswd;
	uint32_t nPublishMs;
};

struct Net_Info {
	FTP_Server ftp_server;
	MQTT_Server mqtt_server;
};

#endif

