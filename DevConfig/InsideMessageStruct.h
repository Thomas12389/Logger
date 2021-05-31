
#ifndef __INSIDE_MESSAGE_STRUCT_H_
#define __INSIDE_MESSAGE_STRUCT_H_

#include <string>

struct Inside_SigStruct {
    uint8_t bIsSend;
    uint8_t bIsSave;
    std::string strOutName;
    std::string strSaveName;
    
    std::string strSigName;
    std::string strSigUnit;
    std::string strSigFormat;
};

#endif
