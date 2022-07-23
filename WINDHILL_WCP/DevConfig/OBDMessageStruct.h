
#ifndef __OBD_MESSAGE_STRUCT_H_
#define __OBD_MESSAGE_STRUCT_H_

#include <string>

enum OBD_ByteOrder : uint8_t {
	OBD_BIG_ENDIAN = 0x00,
	OBD_LITTLE_ENDIAN = 0x01
};

enum OBD_Sig_Type : uint8_t {
    OBD_UNSIGNED = 0x01,
    OBD_SIGNED = 0x02,
    OBD_FLOAT = 0x03,
    OBD_DOUBLE = 0x04
};

struct OBD_FixData {
    double dFactor;
    double dOffset;
    double dDenominator;
    double dFinalOffest;
};

struct OBD_Mode01Signal{
    uint8_t bIsSend;
    uint8_t bIsSave;
    std::string strOutName;
    std::string strSaveName;
    
    std::string strOBDSignalName;
    std::string strOBDSignalUnit;
    std::string strOBDSignalFormat;
    // 起始位相对于一个 PID
    int nStartBit;
    int nLen;
    OBD_ByteOrder nByteOrder;
    OBD_Sig_Type nType;

    OBD_FixData StcFix;
};

struct OBD_Mode01PID {
    uint8_t nPID;
    uint8_t nNumSignal;
    uint8_t nByteSize;
    OBD_Mode01Signal *pOBD_Mode01Signal;
};

struct OBD_Mode01RecvPackage {
    uint8_t nNumPID;
    OBD_Mode01PID *pOBD_PID;
    // uint32_t nResponseID;
};

#endif
