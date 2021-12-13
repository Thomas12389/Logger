/*************************************************************************
	> File Name: CCPMessage.h
	> Author: 
	> Mail: 
	> Created Time: Wed 06 Jan 2021 03:49:33 PM CST
 ************************************************************************/

#ifndef _CCPMESSAGE_H
#define _CCPMESSAGE_H

#include <cinttypes>
#include <array>

typedef uint8_t CCP_BYTE;
typedef uint16_t CCP_WORD;
typedef uint32_t CCP_DWORD;
typedef uint64_t CCP_QWORD;
typedef uint8_t* CCP_BYTEPTR;

typedef CCP_BYTE CCP_CTR;
typedef CCP_WORD CCP_Slave_Addr;
typedef CCP_BYTE CCP_Session_Status;
typedef CCP_DWORD CCP_DTO_ID;
typedef CCP_DWORD CCP_CRO_ID;
typedef CCP_BYTE CCP_Byte_Order;

typedef int32_t CCP_RESULT;

static const CCP_BYTE kCCPPaylodLength{8};
typedef std::array<CCP_BYTE, kCCPPaylodLength> CCP_ByteVector;

// CCP Protocol Version
static const CCP_BYTE CCP_VERSION_MAJOR{0x02};
static const CCP_BYTE CCP_VERSION_MINOR{0x01};

enum CROMsgType : CCP_BYTE {
	/* Basic */
	CC_CONNECT = 0x01,
	CC_SET_MTA = 0x02,
	CC_DOWNLOAD = 0x03,
	CC_UPLOAD = 0x04,
	CC_START_STOP = 0x06,
	CC_DISCONNECT = 0x07,
	CC_GET_DAQ_SIZE = 0x14,
	CC_SET_DAQ_PTR = 0x15,
	CC_WRITE_DAQ = 0x16,
	CC_GET_EXCHANGE_ID = 0x17,
	CC_GET_CCP_VERSION = 0x1B,

	/* Optional */
	CC_TEST = 0x05,
	CC_START_STOP_ALL = 0x08,
	CC_GET_ACTIVE_CAL_PAGE = 0x09,
	CC_SET_S_STATUS = 0x0C,
	CC_GET_S_STATUS = 0x0D,
	CC_BUILD_CHKSUM = 0x0E,
	CC_SHORT_UP = 0x0F,
	CC_CLEAR_MEMORY = 0x10,
	CC_SELECT_CAL_PAGE = 0x11,
	CC_GET_SEED = 0x12,
	CC_UNLOCK = 0x13,
	CC_PROGRAM = 0x18,
	CC_MOVE = 0x19,
	CC_DIAG_SERVICE = 0x20,
	CC_ACTION_SERVICE = 0x21,
	CC_PROGRAM_6 = 0x22,
	CC_DNLOAD_6 = 0x23
};

enum DTOMsgType : CCP_BYTE {
	PID_EVENT = 0xFE,
	PID_CRM = 0xFF
};

enum CRCCodes : CCP_BYTE {
	CRC_OK = 0x00,
	// C0	warning
	CRC_DAQ_OVERLOAD = 0x01,
	// C1	spurious
	// wait(ACK or timeout) -- retries 2
	CRC_CMD_BUSY = 0x10,
	CRC_DAQ_BUSY = 0x11,
	CRC_INTERNAL_TIMEOUT = 0x12,
	CRC_KEY_REQUEST = 0x18,
	CRC_STATUS_REQUEST = 0x19,
	// C2	resolvable
	// reinitialize -- retries 1
	CRC_COLD_START_REQUEST = 0x20,
	CRC_CAL_INIT_REQUEST = 0x21,
	CRC_DAQ_INIT_REQUEST = 0x22,
	CRC_CODE_REQUEST = 0x23,
	// C3	unresolvable
	// terminate -- retries 0
	CRC_CMD_UNKNOWN = 0x30,
	CRC_CMD_SYNTAX = 0x31,
	CRC_OUT_OF_RANGE = 0x32,
	CRC_ACCESS_DENITED = 0x33,
	CRC_OVERLOAD = 0x34,
	CRC_ACCESS_LOCKED = 0x35,
	CRC_NOT_AVAILABLE = 0x36
};
// Session Status
enum SessionStatus : CCP_BYTE {
	SS_CAL = 0x01,
	SS_DAQ = 0x02,
	SS_RESUME = 0x04,
	//
	SS_TMP_DISCONNECTION = 0x10,
	SS_CONNECTED = 0x20,
	//
	SS_STORE = 0x40,
	SS_RUN = 0x80
};
// Start/Stop Mode
enum Mode : CCP_BYTE {
	MODE_STOP = 0x00,
	MODE_START = 0x01,
	MODE_PRE = 0x02
};

enum CCPSendStatus : CCP_BYTE {
	CCP_FREE = 0x01,
	CCP_CRM_PENDING = 0x02
};


// structure definitions
struct CCP_Message {
	int Channel;
	int CANID;
	CCP_BYTE Data[8];
};

struct CCP_ExchangeData {
	CCP_BYTE IdLength;			// Length of the Slave Device ID
	CCP_BYTE DataType;			// Data type qualifier of the Slave Device ID
	CCP_BYTE AvailabilityMask;	// Resource Availability Mask
	CCP_BYTE ProtectionMask;	// Resource Protection Mask
};

struct CCP_StartStopData {
	CCP_BYTE Mode;							// 0: Stop, 1: Start
	CCP_BYTE ListNumber;					// DAQ list number to process
	CCP_BYTE LastODTNumber;					// ODTS to br transmitted (from 0 to byLastODTNumber)
	CCP_BYTE EventChannel;					// Generic signal source for timing determination
	CCP_BYTE TransmissionRatePrescaler;		// Transmission Rate Prescaler
};

struct CCP_SlaveData {
	uint32_t nEcuAddress;		// Station address (Intel Format)
	uint32_t nIdCRO;		    // CAN Id used for CRO pacjages (29 Bits = MSB set)
	uint32_t nIdDTO;		    // CAN Id used for DTO pacjages (29 Bits = MSB set)
	bool bIntelFormat;			// Format used by the salve (True: Intel, False: Motorola)
    uint16_t nProVersion;    	// CCP protocol version used by salve
	// 重载 < 运算符， 方便使用 map
	bool operator <(const CCP_SlaveData& a) const {
		return nIdDTO < a.nIdDTO;
	}
};

#endif
