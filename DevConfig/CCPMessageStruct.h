
#ifndef _CCPMESSAGESTRUCT_H__
#define _CCPMESSAGESTRUCT_H__

#include "CCP/CCPMessage.h"

#include <cstdio>
#include <map>
#include <vector>

enum CCP_ByteOrder : uint8_t {
	CCP_BIG_ENDIAN = 0x00,
	CCP_LITTLE_ENDIAN = 0x01
};

enum CCP_Ele_Type : uint8_t {
    CCP_UNSIGNED = 0x01,
    CCP_SIGNED = 0x02,
    CCP_FLOAT = 0x03
};

struct CCP_FixData {
    uint32_t cMask;
    double dFactor;
    double dOffset;
};

struct CCP_ElementStruct {
    uint32_t nAddress;
    uint8_t nAddress_Ex;
    uint8_t nEleNO;
    uint8_t nLen;
    CCP_Ele_Type nType;

    CCP_ByteOrder nByteOrder;
    CCP_FixData StcFix;
    // 标识是否需要输出
    uint8_t bIsSend;
    uint8_t bIsSave;
    std::string strOutName;
    std::string strSaveName;

    std::string strElementName;
    std::string strElementFormat;
    std::string strElementUnit;
};

struct CCP_ODT {
    uint8_t nOdtNO;
    uint8_t nNumElements;
    CCP_ElementStruct *pElement;
};

struct CCP_DAQList {
    uint8_t nDaqNO;
    uint8_t nEventChNO;
    uint8_t nPrescaler;
    uint8_t nOdtNum;
    uint8_t nFirstPID;
    CCP_ODT *pODT;
    uint32_t nCANID;
};

#endif