/*************************************************************************
	> File Name: XCPMessage.h
	> Author: 
	> Mail: 
	> Created Time: Wed 22 Dec 2021 17:49:33 PM CST
 ************************************************************************/

#ifndef _XCPMESSAGE_H
#define _XCPMESSAGE_H

#include <cinttypes>
#include <array>

typedef uint8_t XCP_BYTE;
typedef uint16_t XCP_WORD;
typedef uint32_t XCP_DWORD;
typedef uint64_t XCP_DLONG;
typedef uint8_t* XCP_BYTEPTR;

typedef XCP_DWORD XCP_Broadcast_ID;
typedef XCP_DWORD XCP_Slave_ID;
typedef XCP_DWORD XCP_Master_ID;
typedef XCP_BYTE XCP_Byte_Order;
typedef XCP_BYTE XCP_Session_Status;

typedef int32_t XCP_RESULT;

static const XCP_BYTE kMaxXCPPaylodLength{64};
typedef std::array<XCP_BYTE, kMaxXCPPaylodLength> XCP_ByteVector;

// CCP Protocol Version
static const XCP_BYTE XCP_VERSION_MAJOR{0x01};
static const XCP_BYTE XCP_VERSION_MINOR{0x05};

enum M2SType : XCP_BYTE {
	/* Command */
	// standard
	XCP_CONNECT = 0xFF,
	XCP_DISCONNECT = 0xFE,
	XCP_GET_STATUS = 0xFD,
	XCP_SYNCH = 0xFC,
	XCP_TRANSPORT_LAYER_CMD = 0xF2,

	XCP_GET_VERSION = 0xC0,
	XCP_SET_DAQ_PACKED_MODE = 0xC0,

	// basic data acquisition and stimulation
	XCP_SET_DAQ_PTR = 0xE2,
	XCP_WRITE_DAQ = 0xE1,
	XCP_SET_DAQ_LIST_MODE = 0xE0,
	XCP_START_STOP_DAQ_LIST = 0xDE,
	XCP_GET_DAQ_RESOLUTION_INFO = 0xD9,
	XCP_GET_DAQ_PROCESSOR_INFO = 0xDA,
	XCP_START_STOP_SYNCH = 0xD0,

	// static data acquisition and stimulation
	XCP_CLEAR_DAQ_LIST = 0xE3,

	// dynamic data acquisition and stimulation
	XCP_FREE_DAQ = 0xD6,
	XCP_ALLOC_DAQ = 0xD5,
	XCP_ALLOC_ODT = 0xD4,
	XCP_ALLOC_ODT_ENTRY = 0xD3,

	/* STIM */
	XCP_MAX_STIM = 0xBF
};

enum S2MType : XCP_BYTE {
	XCP_RES = 0xFF,
	XCP_ERR = 0xFE,
	XCP_EV = 0xFD,
	XCP_SERV = 0xFC,
	XCP_MAX_DAQ = 0xFB
};

enum XCPErrorCodes : XCP_BYTE {
	// S0
	XCP_ERR_CMD_SYNCH = 0x00,
	// S2
	XCP_ERR_CMD_BUSY = 0x10,
	XCP_ERR_DAQ_ACTIVE = 0x11,
	XCP_ERR_PGM_ACTIVE = 0x12,
	XCP_CMD_UNKNOWN = 0x20,
	XCP_CMD_SYNTAX = 0x21,
	XCP_ERR_OUT_OF_RANGE = 0x22,
	XCP_ERR_WRITE_PROTECTED = 0x23,
	XCP_ACCESS_DENIED = 0x24,
	XCP_ERR_ACCESS_LOCKED = 0x25,
	XCP_ERR_PAGE_NOT_VALID = 0x26,
	XCP_ERR_MODE_NOT_VALID = 0x27,
	XCP_ERR_SEGMENT_NOT_VALID = 0x28,
	XCP_ERR_SEQUENCE = 0x29,
	XCP_ERR_DAQ_CONFIG = 0x2A,
	XCP_ERR_MEMORY_OVERFLOW = 0x30,
	XCP_ERR_GENGRIC = 0x31,
	XCP_ERR_RESOURCE_TEMPORARY_NOT_ACCESSIBLE = 0x33,
	XCP_ERR_SUBCMD_UNKNOWN = 0x34,
	XCP_ERR_TIMECORR_STATE_CHANGE = 0x35,
	// S3
	XCP_ERR_VERIFY = 0x32,

};
// Session Status
enum XCPSessionStatus : XCP_BYTE {
	XCP_SES_RESET_SESSION = 0x00,
	XCP_SES_STORE_CAL_REQ = 0x01,
	XCP_SES_CONNECTED = 0x02,
	XCP_SES_STORE_DAQ_REQ = 0x04,
	XCP_SES_CLEAR_DAQ_REQ = 0x08,
	XCP_SES_RESERVE2 = 0x10,
	XCP_SES_RESERVE3 = 0x20,
	XCP_SES_DAQ_RUNNING = 0x40,
	XCP_SES_RESUME = 0x80,
};
// Start/Stop DAQList Mode
enum XCPStartStopMode : XCP_BYTE {
	STOP = 0x00,
	START = 0x01,
	SELECT = 0x02
};

// Start/Stop Synch Mode
enum XCPStartStopSynchMode : XCP_BYTE {
	STOP_ALL = 0x00,
	START_SELECTED = 0x01,
	STOP_SELECTED = 0x02,
	PREPARE_START_SELECTED = 0x03
};

enum XCPDAQConfigType : XCP_BYTE {
	STATIC_DAQ = 0x00,
	DYNAMIC_DAQ = 0x01
};

enum XCPSendStatus : XCP_BYTE {
	XCP_FREE = 0x01,
	XCP_S2M_PENDING = 0x02
};


// structure definitions
struct XCP_Message {
	int Channel;
	int CANID;
	XCP_BYTE Length;
	XCP_BYTE Data[64];
};

struct XCP_SlaveData {
	XCP_Broadcast_ID nBroadcastID;
	XCP_Master_ID nMasterID;
	XCP_Slave_ID nSlaveID;
	bool bMaxDLCRequired;
	bool bIntelFormat;			// Format used by the slave (True: Intel, False: Motorola)
    uint16_t nProVersion;    	// XCP protocol version used by slave

	bool CAL_PG;
	bool DAQ;
	bool STIM;
	bool PGM;

	uint8_t AddressGranularity;
	bool SlaveBlockMode;
	bool OptionalData;

	uint16_t MaxDto;
	uint8_t MaxCto;

	struct {
		bool ConfigType;
		bool PrescalerSupported;
		bool ResumeSupported;
		bool BitStimSupported;
		bool TimeStampSupported;
		bool PidOffSupported;
		uint8_t OverloadIndicationMode;
		uint8_t OptimisationType;
		uint8_t AddressExtensionType;
		uint8_t IdentificationFieldType;
		uint16_t MaxDaq;
		uint8_t MaxEventChannel;
		uint8_t MinDaq;
	} DaqProperies;

	// 重载 < 运算符， 方便使用 map
	bool operator <(const XCP_SlaveData& a) const {
		return nSlaveID < a.nSlaveID;
	}
};

#endif
