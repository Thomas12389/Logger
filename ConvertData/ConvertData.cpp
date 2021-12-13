
#include <iostream>
#include <inttypes.h>

#include "ConvertData.h"

#include "DevConfig/ParseXmlCAN.hpp"
#include "DevConfig/PhyChannel.hpp"

#include "Logger/Logger.h"
#include "datatype.h"

#define Motorola_LSB

extern Channel_MAP g_Channel_MAP;
stu_OutMessage g_stu_OutMessage;
stu_SaveMessage g_stu_SaveMessage;

#ifdef SELF_OBD
int can_obd_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData) {

    if (NULL == ChanName) {
        return -1;
    }

    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
    if (ItrChan == g_Channel_MAP.end()) {
        XLOG_TRACE("WARN: obd_convert: channel {} not exist.", ChanName);
        return -1;
    }

    // OBD 检查 ID
    OBD_MAP_MsgID_RMsgPkg::iterator ItrOBD = (ItrChan->second).CAN.mapOBD.find(nCANID);
    if (ItrOBD == (ItrChan->second).CAN.mapOBD.end()) {
        XLOG_TRACE("WARN: obd_convert: channel {} dosen't exist ID:{}.", ChanName, nCANID);
        return -1;
    }

    // 检查 functionID
    OBD_RecvPackage *pOBDRevPackInfo = ItrOBD->second;
    if (NULL == pOBDRevPackInfo) {
        return -1;
    }
    OBD_Function *pOBD_Function = pOBDRevPackInfo->pOBD_Function;
    uint8_t nIdxFunction = 0;
    uint8_t nFunctionID = pMsgData[1] - 0x40;
    for (; nIdxFunction < pOBDRevPackInfo->nNumFunction; nIdxFunction++) {
        if (pOBD_Function[nIdxFunction].nFunctionID == nFunctionID) {
            break;
        }
    }
    if (nIdxFunction == pOBDRevPackInfo->nNumFunction) {
        XLOG_TRACE("WARN: obd_convert: channel {} ID:{} dosen't exist function:{}.", ChanName, nCANID, nFunctionID);
        return -1;
    }

    // 检查 subfunctionID
    OBD_SubFunc *pOBD_SubFunc = pOBD_Function[nIdxFunction].pOBD_SubFunc;
    uint8_t nIdxSubFunc = 0;
    uint8_t nSubFuncID = pMsgData[2];
    for (; nIdxSubFunc < pOBD_Function[nIdxFunction].nNumSubFunction; nIdxSubFunc++) {
        if (pOBD_SubFunc[nIdxSubFunc].nSubFuncID == nSubFuncID) {
            break;
        }
    }
    if (nIdxSubFunc == pOBD_Function[nIdxFunction].nNumSubFunction) {
        XLOG_TRACE("WARN: obd_convert: channel {} ID:{} function:{} dosen't exist subfunc:{}.", ChanName, nCANID, nFunctionID, nSubFuncID);
        return -1;
    }

    if (!pOBD_SubFunc[nIdxSubFunc].bIsSave && !pOBD_SubFunc[nIdxSubFunc].bIsSend) return 0;
    // convert data
    uint8_t eff_len = pMsgData[0] - 2;  // 数据有效长度，减去 functionID 和 subfunctionID 所占字节
    int64_t lRet = 0;
    uint8_t nStartByte = 3;
    uint8_t nByteCount = eff_len;
    while (nByteCount--) {
        lRet <<= 8;
        lRet |= pMsgData[nStartByte++];
    }

    double dFactor = pOBD_SubFunc[nIdxSubFunc].StcFix.dFactor;
    double dOffest = pOBD_SubFunc[nIdxSubFunc].StcFix.dOffset;
    double dDenominator = pOBD_SubFunc[nIdxSubFunc].StcFix.dDenominator;
    double dPhyValue = (lRet * dFactor + dOffest) / dDenominator;

    // printf("obd %s value = %f\n", pOBD_SubFunc[nIdxSubFunc].strOutName.c_str(), dPhyValue);
    Out_Message Msg;
    Msg.strName = pOBD_SubFunc[nIdxSubFunc].strOutName;
    Msg.dPhyVal = dPhyValue;
    Msg.strPhyUnit = pOBD_SubFunc[nIdxSubFunc].strOBDSubFuncUnit;
    Msg.strPhyFormat = pOBD_SubFunc[nIdxSubFunc].strOBDSubFuncFormat;
    // 写输出信号 map
    if (pOBD_SubFunc[nIdxSubFunc].bIsSend) {
        std::lock_guard<std::mutex> lock(g_stu_OutMessage.msg_lock);
        write_msg_out_map(pOBD_SubFunc[nIdxSubFunc].strOutName, Msg);
    }
    // 写存储信号 map
    if (pOBD_SubFunc[nIdxSubFunc].bIsSave) {
        std::lock_guard<std::mutex> lock(g_stu_SaveMessage.msg_lock);
        write_msg_save_map(pOBD_SubFunc[nIdxSubFunc].strSaveName, Msg);
    }

    return 0;    
}
#endif

int can_dbc_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData) {
    if (NULL == ChanName) {
        return -1;
    }

    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
    if (ItrChan == g_Channel_MAP.end()) {
        XLOG_TRACE("WARN: dbc_convert: channel {} not exist.", ChanName);
        return -1;
    }

    // 普通 CAN 信号（DBC）
    COM_MAP_MsgID_RMsgPkg::iterator ItrCOM = (ItrChan->second).CAN.mapDBC.find(nCANID);
    if (ItrCOM == (ItrChan->second).CAN.mapDBC.end()) return 0;
    
    ReceiveMessagePack *pRevPackInfo = ItrCOM->second;
    if (NULL == pRevPackInfo) {
        return -1;
    }

    // convert data
    DBC_SigStruct *pSig = pRevPackInfo->pPackSig;
    for (int idxSig = 0; idxSig < pRevPackInfo->nNumSigs; idxSig++) {
        // 该信号不上传不保存，则不进行解析
        if (!pSig[idxSig].bIsSave && !pSig[idxSig].bIsSend) continue;

        uint64_t lRet = 0;
        DBC_ByteOrder nOrder = pSig[idxSig].nByteOrder;
        int nLen = pSig[idxSig].nLen;    // get the signal length
        int nStartIndex = pSig[idxSig].nStartBit / 8;  // get the sequence number of the byte in which the start bit resides
        uint8_t u8DataStart = (pMsgData[nStartIndex]);  // get the value of the byte in which start bit resides
        int nStartOffest = pSig[idxSig].nStartBit % 8;   // get the position of the start bit in the corrdsponding byte

        // 起始字节
        int nLeft = 0;
        if (DBC_Intel == nOrder) {
            u8DataStart >>= nStartOffest;       // get the value from start bit to the 8th bit of the corresponding byte
            nLeft = nLen - (8 - nStartOffest);  // get the remaining bit length after removing the length of nStartOffest
        } else if (DBC_Motorola == nOrder) {
#ifdef Motorola_LSB
            u8DataStart >>= nStartOffest;
            nLeft = nLen - (8 - nStartOffest);
#else
            u8DataStart &= (0xFF >> (7 - nStartOffest));       // get the value from start bit to the 1th bit of the corresponding byte
            nLeft = nLen - (nStartOffest + 1);  // get the remaining bit length after removing the length of nStartOffest
#endif
        }

        if (nLeft > 0) {        // the signal cross a byte
            // 中间整字节
            lRet |= u8DataStart;
            int nByteCount = nLeft / 8;     // get the number of bytes of the remaining bit length
            int tempByteCount = nByteCount;

            uint64_t temp = 0;
            if (DBC_Intel == nOrder) {
                /* Intel */
                while (nByteCount--) {
                    temp = ((uint64_t)pMsgData[++nStartIndex]) << (((tempByteCount - nByteCount) << 3) - nStartOffest);
                    lRet |= temp;
                } 
            } else if (DBC_Motorola == nOrder) {
                /* Motorola */ // TODO
                while (nByteCount--) {
#ifdef Motorola_LSB
                    temp = ((uint64_t)pMsgData[--nStartIndex]) << (((tempByteCount - nByteCount) << 3) - nStartOffest);
                    lRet |= temp;
#else
                    lRet <<= 8;
                    lRet |= pMsgData[++nStartIndex];
#endif                    
                }
            }
            // 最后剩余位数
            int nEndLength = nLeft % 8;     // the remaining bit length in addition to the whole byte
            if (nEndLength) {

                if (DBC_Intel == nOrder) {
                    uint8_t u8DataEnd = pMsgData[++nStartIndex];
                    u8DataEnd &= (0xFF >> (8 - nEndLength));
                    uint64_t temp = ((uint64_t)u8DataEnd) << (nLen - nEndLength);
                    lRet |= temp;
                } else if (DBC_Motorola == nOrder) {
#ifdef Motorola_LSB
                    uint8_t u8DataEnd = pMsgData[--nStartIndex];
                    u8DataEnd &= (0xFF >> (8 - nEndLength));
                    uint64_t temp = ((uint64_t)u8DataEnd) << (nLen - nEndLength);
                    lRet |= temp;
#else
                    uint8_t u8DataEnd = pMsgData[++nStartIndex];
                    u8DataEnd >>= (8 - nEndLength);printf("lRet = %llx\n", lRet);
                    lRet <<= nEndLength;
                    lRet |= u8DataEnd;
#endif
                }
            }
        } else {        // the signal is in one byte
            u8DataStart &= (0xFF >> (8 - nLen));
            lRet |= u8DataStart;
        }
        
        double dFactor = pSig[idxSig].StcFix.dFactor;
        double dOffset = pSig[idxSig].StcFix.dOffset;
        double dPhyValue = SIGNAL_NAN;

        // 有符号整型补全符号位时要注意，除了有效位其余位均需填充(算术右移)
        if (DBC_SIGNED == pSig[idxSig].nType) {
            uint64_t bSign = (lRet >> (nLen - 1)) & 0x01;  // 符号位
            if (nLen > 32) {
                uint64_t mask = (bSign == 1 ? 0xFFFFFFFFFFFFFFFF << nLen : 0);
                dPhyValue = ((int64_t)(lRet | mask )) * dFactor + dOffset;
            } else if (nLen > 16) {
                uint32_t mask = (bSign == 1 ? 0xFFFFFFFF << nLen : 0);
                dPhyValue = ((int32_t)(lRet | mask )) * dFactor + dOffset;
            } else if (nLen > 8) {
                uint16_t mask = (bSign == 1 ? 0xFFFF << nLen : 0);
                dPhyValue = ((int16_t)(lRet | mask )) * dFactor + dOffset;
            } else {
                uint8_t mask = (bSign == 1 ? 0xFF << nLen : 0);
                dPhyValue = ((int8_t)(lRet | mask )) * dFactor + dOffset;
            }
        } else if (DBC_UNSIGNED == pSig[idxSig].nType) {
            dPhyValue = ((uint64_t)lRet) * dFactor + dOffset;
        } else if (DBC_FLOAT == pSig[idxSig].nType) {
            void *temp = &lRet;
            dPhyValue = (*(float *)(temp)) * dFactor + dOffset;
        } else if (DBC_DOUBLE == pSig[idxSig].nType) {
            void *temp = &lRet;
            dPhyValue = (*(double *)(temp)) * dFactor + dOffset;
        }

        Out_Message Msg;
        Msg.strName = pSig[idxSig].strOutName;
        Msg.dPhyVal = dPhyValue;
        Msg.strPhyUnit = pSig[idxSig].strSigUnit;
        Msg.strPhyFormat = pSig[idxSig].strSigFormat;
        // 写输出信号 map
        if (pSig[idxSig].bIsSend) {
            std::lock_guard<std::mutex> lock(g_stu_OutMessage.msg_lock);
            write_msg_out_map(pSig[idxSig].strOutName, Msg);
        }
        // 写存储信号 map
        if (pSig[idxSig].bIsSave) {
            std::lock_guard<std::mutex> lock(g_stu_SaveMessage.msg_lock);
            write_msg_save_map(pSig[idxSig].strSaveName, Msg);
        }
#if 0            
        std::cout << "Channel name = " << ChanName << "  MsgID = " << std::hex << ItrCOM->first << std::endl;
        std::cout << "i = " << idxSig << ", pSig[idxSig].strOutName : " << pSig[idxSig].strOutName;
        std::cout << ", dPhyValue = " << dPhyValue << std::endl << std::endl;
#endif
    }
    return 0;
}

int can_ccp_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData) {
    if (NULL == ChanName) {
        return -1;
    }

    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
    if (ItrChan == g_Channel_MAP.end()) {
        XLOG_TRACE("WARN: ccp_convert: channel {} not exist.", ChanName);
        return -1;
    }

    // 检查 ID
    CCP_MAP_Salve_DAQList::iterator ItrCCP = (ItrChan->second).CAN.mapCCP.begin();
    for (; ItrCCP != (ItrChan->second).CAN.mapCCP.end(); ItrCCP++) {
        std::vector<CCP_DAQList>::iterator ItrVecDAQ = (ItrCCP->second).begin();
        for (; ItrVecDAQ != (ItrCCP->second).end(); ItrVecDAQ++) {
            // printf("(*ItrVecDAQ).nCANID = 0x%02X\n", (*ItrVecDAQ).nCANID);
            // printf("nCANID = 0x%02X\n", nCANID);
            if ((*ItrVecDAQ).nCANID != nCANID) continue;
            break;
        }
        if (ItrVecDAQ == (ItrCCP->second).end()) continue;
        break;
    }
    if (ItrCCP == (ItrChan->second).CAN.mapCCP.end()) {
        XLOG_TRACE("WARN: ccp_convert: channel {} dosen't exist DAQList_ID:{}.", ChanName, nCANID);
        return -1;
    }

    // 检查 PID
    std::vector<CCP_DAQList>::iterator ItrVecDAQ = (ItrCCP->second).begin();
    CCP_ODT *pODT = (*ItrVecDAQ).pODT;
    uint8_t nIdxODT = 0;
    uint8_t nPID = pMsgData[0];
    for (; ItrVecDAQ != (ItrCCP->second).end(); ItrVecDAQ++) {
        pODT = (*ItrVecDAQ).pODT;
        nIdxODT = 0;
        for (; nIdxODT < (*ItrVecDAQ).nOdtNum; nIdxODT++) {
            if ((*ItrVecDAQ).nFirstPID + nIdxODT != nPID) continue;
            break;
        }
        if (nIdxODT == (*ItrVecDAQ).nOdtNum) continue;
        break;
    }
    if (ItrVecDAQ == (ItrCCP->second).end()) {
        XLOG_TRACE("WARN: ccp_convert: channel {} DAQList_ID:{} dosen't exist PID {}.", ChanName, nCANID, nPID);
        return -1;
    }
    
    // 数据转换
    CCP_ElementStruct *pEle = pODT[nIdxODT].pElement;
    uint8_t nUsedLen = 1;      // 已经使用的字节数， PID 占一字节
    for (int idxEle = 0; idxEle < pODT[nIdxODT].nNumElements; idxEle++) {
        pEle = pEle + idxEle;       // 遍历到第几个 Element
        if (!pEle->bIsSave && !pEle->bIsSend) continue;
        
        int nStartByte = nUsedLen;      // 从第几个字节开始
        int nEndByte = nStartByte + pEle->nLen - 1;     // 到第几个字节结束
        int nByteCount = pEle->nLen;

        int32_t lRet = 0;
        if (nByteCount > 1) {
            while (nByteCount--) {
                if (pEle->nByteOrder == CCP_LITTLE_ENDIAN) {
                    // 高字节
                    int32_t temp = ((int32_t)pMsgData[nEndByte--]) << ((nByteCount) << 3);
                    lRet |= temp;
                    // printf("nByteCount = %d, lRet = %#x\n", nByteCount, lRet);
                } else {
                    lRet <<= 8;
                    lRet |= pMsgData[nStartByte++];
                }
            }
            lRet &= pEle->StcFix.cMask;
        } else {
            lRet = pMsgData[nStartByte] & pEle->StcFix.cMask;
        }
        // printf("lRet = %#x\n", lRet);

        double dFactor = pEle->StcFix.dFactor;
        double dOffest = pEle->StcFix.dOffset;
        double dPhyValue = 0.0;

        if (pEle->nType == CCP_SIGNED) {
            uint64_t bSign = (lRet >> (8 * pEle->nLen - 1)) & 0x01;  // 符号位
            if (pEle->nLen == 1) {
                dPhyValue = (int8_t)(lRet | bSign << 7) * dFactor + dOffest;
            } else if (pEle->nLen == 2) {
                dPhyValue = (int16_t)(lRet | bSign << 15) * dFactor + dOffest;
            } else if (pEle->nLen == 4) {
                dPhyValue = (int32_t)(lRet | bSign << 31) * dFactor + dOffest;
            }
        } else if (pEle->nType == CCP_UNSIGNED) {
            dPhyValue = (uint32_t)lRet * dFactor + dOffest;
        } else if (pEle->nType == CCP_FLOAT) {
            void *temp = &lRet;
            dPhyValue = (*(float *)(temp)) * dFactor + dOffest;
        }

        Out_Message Msg;
        Msg.strName = pEle->strOutName;
        Msg.dPhyVal = dPhyValue;
        Msg.strPhyUnit = pEle->strElementUnit;
        Msg.strPhyFormat = pEle->strElementFormat;
        if (pEle->bIsSend) {
            std::lock_guard<std::mutex> lock(g_stu_OutMessage.msg_lock);
            write_msg_out_map(pEle->strOutName, Msg);
        }
        if (pEle->bIsSave) {
            std::lock_guard<std::mutex> lock(g_stu_SaveMessage.msg_lock);
            write_msg_save_map(pEle->strSaveName, Msg);
        }
        nUsedLen += pEle->nLen;
#if 0
        std::cout << "Channel name = " << ChanName << "  MsgID = " << std::hex << (*ItrVecDAQ).nCANID << std::endl;
        std::cout << "idxEle = " << idxEle << ", pEle->StrOutName : " << pEle->strOutName;
        std::cout << ", dPhyValue = " << dPhyValue << std::endl << std::endl;
#endif
    }
    return 0;
}
            
int write_msg_out_map(const std::string strOutName, const Out_Message& MsgOut) {
    if (!strOutName.empty()) {

        // LIST_Message::iterator ItrList = g_stu_OutMessage.msg_struct.msg_list.begin();
        LIST_Message& List = g_stu_OutMessage.msg_struct.msg_list;
        LIST_Message::iterator ItrList = List.begin();

        for (; ItrList != List.end(); ItrList++) {
            if (strOutName != (*ItrList).strName) continue;
            // 修改
            *ItrList = MsgOut;
            break;
        }
    }

    return 0;
}

int write_msg_save_map(const std::string strSaveName, const Out_Message& MsgSave) {
    if (!strSaveName.empty()) {
        LIST_Message& List = g_stu_SaveMessage.msg_struct.msg_list;
        LIST_Message::iterator ItrList = List.begin();

        for (; ItrList != List.end(); ItrList++) {
            if (strSaveName != (*ItrList).strName) continue;
            // 修改
            *ItrList = MsgSave;
            break;
        }
    }
    return 0;
}