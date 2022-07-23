
#ifndef _XCPMESSAGESTRUCT_H__
#define _XCPMESSAGESTRUCT_H__

#include "XCP/XCPMessage.h"

#include <cstdio>
#include <map>
#include <vector>

enum XCP_ByteOrder : uint8_t {
	XCP_BIG_ENDIAN = 0x00,
	XCP_LITTLE_ENDIAN = 0x01
};

enum XCP_Ele_Type : uint8_t {
    XCP_UNSIGNED = 0x01,
    XCP_SIGNED = 0x02,
    XCP_FLOAT = 0x03
};

struct XCP_FixData {
    uint32_t cMask;
    double dFactor;
    double dOffset;
};

struct XCP_ElementStruct {
    uint32_t nAddress;
    uint8_t nAddress_Ex;
    uint8_t nEleNO;
    uint8_t nLen;
    XCP_Ele_Type nType;

    XCP_ByteOrder nByteOrder;
    XCP_FixData StcFix;
    // 标识是否需要输出
    uint8_t bIsSend;
    uint8_t bIsSave;
    std::string strOutName;
    std::string strSaveName;

    std::string strElementName;
    std::string strElementFormat;
    std::string strElementUnit;
};

struct XCP_ODT {
    uint8_t nOdtNO;
    uint8_t nNumElements;
    XCP_ElementStruct *pElement;
};

struct XCP_DAQList {
    uint16_t nDaqNO;
    uint8_t nMode;
    uint16_t nEventChNO;
    uint8_t nPrescaler;
    uint8_t nPriority;
    uint8_t nFirstPID;
    uint32_t LastTimestamp;
    uint8_t nOdtNum;
    XCP_ODT *pODT;
    uint32_t nCANID;
};

#endif