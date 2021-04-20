
#ifndef __DBC_MESSAGE_STRUCT_H_
#define __DBC_MESSAGE_STRUCT_H_

#include <string>

struct FixData {
    double dFactor;
    double dOffset;
};

struct SigStruct {
    uint8_t bIsSend;
    uint8_t bIsSave;
    std::string strOutName;
    std::string strSaveName;
    
    std::string strSigName;
    std::string strSigUnit;
    std::string strSigFormat;
    int nStartBit;
    int nLen;
    FixData StcFix;
};

enum E_FRAME_TYPE{E_FRAME_STD, E_FRAME_EXT, E_FRAME_UNKNOWN};//Frame type:standard extend

struct ReceiveMessagePack {
    E_FRAME_TYPE e_Frame_Type;
    int nDataLen;
    int nNumSigs;
    SigStruct *pPackSig;
};

#endif
