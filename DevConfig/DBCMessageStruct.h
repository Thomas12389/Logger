
#ifndef __DBC_MESSAGE_STRUCT_H_
#define __DBC_MESSAGE_STRUCT_H_

#include <string>

enum DBC_ByteOrder : uint8_t {
	DBC_Motorola = 0x00,
	DBC_Intel = 0x01
};

enum DBC_Sig_Type : uint8_t {
    DBC_UNSIGNED = 0x07,
    DBC_SIGNED = 0x08,
    DBC_FLOAT = 0x09,
    DBC_DOUBLE = 0x0A
};

struct DBC_FixData {
    double dFactor;
    double dOffset;
};

struct DBC_SigStruct {
    uint8_t bIsSend;
    uint8_t bIsSave;
    std::string strOutName;
    std::string strSaveName;
    
    std::string strSigName;
    std::string strSigUnit;
    std::string strSigFormat;
    int nStartBit;
    int nLen;
    DBC_ByteOrder nByteOrder;
    DBC_Sig_Type nType;
    DBC_FixData StcFix;
};

enum E_FRAME_TYPE{E_FRAME_STD, E_FRAME_EXT, E_FRAME_UNKNOWN};//Frame type:standard extend

struct ReceiveMessagePack {
    E_FRAME_TYPE e_Frame_Type;
    int nDataLen;
    int nNumSigs;
    DBC_SigStruct *pPackSig;
};

#endif
