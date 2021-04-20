
#ifndef __OBD_MESSAGE_STRUCT_H_
#define __OBD_MESSAGE_STRUCT_H_

#include <string>

struct OBD_FixData {
    double dFactor;
    double dOffset;
    double dDenominator;
};

struct OBD_SubFunc{
    uint8_t nSubFuncID;

    uint8_t bIsSend;
    uint8_t bIsSave;
    std::string strOutName;
    std::string strSaveName;
    
    std::string strOBDSubFuncName;
    std::string strOBDSubFuncUnit;
    // need?
    std::string strOBDSubFuncFormat;

    OBD_FixData StcFix;
};

struct OBD_Function {
    uint8_t nFunctionID;
    uint8_t nNumSubFunction;
    OBD_SubFunc *pOBD_SubFunc;
};

struct OBD_RecvPackage {
    uint8_t nNumFunction;
    OBD_Function *pOBD_Function;
    uint32_t nCANID;
};

#endif
