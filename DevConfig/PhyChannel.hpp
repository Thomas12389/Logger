
#ifndef __PHYCHANNEL_H__
#define __PHYCHANNEL_H__

#include "DevConfig/ParseXmlCAN.hpp"
#include "DevConfig/ParseXmlCAN_DBC.hpp"
#include "DevConfig/ParseXmlCAN_CCP.hpp"
#include "DevConfig/ParseXmlCAN_OBD.hpp"
#include "DevConfig/ParseXmlInside.hpp"

struct Channel_Info {
	struct CAN{
		CAN_BuardRate stuBuardRate;
		COM_MAP_MsgID_RMsgPkg mapDBC;
		CCP_MAP_Salve_DAQList mapCCP;
		OBD_MAP_MsgID_RMsgPkg mapOBD;
	} CAN;
};

// Channel 
typedef std::map<std::string, Channel_Info> Channel_MAP;

struct Inside_Message{
	uint8_t isEnable;
	int nNumSigs;
	Inside_SigStruct* pInsideSig;
};

typedef std::map<std::string, Inside_Message> Inside_MAP;		// string -- GPS or Internal

// device 
struct Hardware_Info {
	std::string SeqNum;
};

// File
struct File_Save {
	std::string strLocalDirName;
	std::string strRemoteDirName;
	std::string strRemoteFileName;
	uint32_t nSaveCycleMs;
};

// Net
struct FTP_Server {
    std::string strIP;
    uint16_t nPort;
    std::string strUserName;
    std::string strPasswd;
};

struct MQTT_Server {
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

