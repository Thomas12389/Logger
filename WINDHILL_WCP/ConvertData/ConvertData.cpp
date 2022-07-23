
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

typedef struct g_con_lock {
    std::mutex dbc_con_lock;
    std::mutex ccp_con_lock;
    std::mutex xcp_con_lock;
    std::mutex obd_con_lock;
} g_con_lock;

// pMsgData 为去掉 PCI 部分的有效数据
int can_obd_convert2msg(const char *ChanName, uint32_t nREQID, uint32_t nRSPID, std::list<uint8_t> lPID , uint8_t nDataLen, uint8_t *pMsgData) {
    if (NULL == ChanName) {
        return -1;
    }

    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
    if (ItrChan == g_Channel_MAP.end()) {
        XLOG_TRACE("WARN: obd_convert: channel {} not exist.", ChanName);
        return -1;
    }

    OBD_MAP_MsgID_Mode01_RMsgPkg::iterator ItrOBD = (ItrChan->second).CAN.OBDII.mapOBD.find(nREQID);
    if (ItrOBD == (ItrChan->second).CAN.OBDII.mapOBD.end()) {
        XLOG_TRACE("WARN: obd_convert: channel {}, REQID {} not exist.", ChanName, nREQID);
        return -1;
    }

    for (; ItrOBD != (ItrChan->second).CAN.OBDII.mapOBD.end(); ItrOBD++) {
        // 查找 ResponseID
        std::map<uint32_t, OBD_Mode01RecvPackage*>::iterator ItrRSP = (ItrOBD->second).find(nRSPID);
        if (ItrRSP == (ItrOBD->second).end()) return 0;

        OBD_Mode01RecvPackage *pOBD_Mode01RecvPackage = ItrRSP->second;
        if (NULL == pOBD_Mode01RecvPackage) return -1;

        // 解析数据
        uint8_t idxDataPIDByte = 0;    // PID 所在的字节
        uint8_t nPID = 0;
        OBD_Mode01PID *pPID = pOBD_Mode01RecvPackage->pOBD_PID;
        if (NULL == pPID) return -1;

        for(int idxPID = 0; idxPID < pOBD_Mode01RecvPackage->nNumPID; idxPID++) {

            if (idxDataPIDByte >= nDataLen) break;
            // 获取 PID
            nPID = pMsgData[idxDataPIDByte];
            // 若该 PID 不是请求当中的，则直接结束
            if (std::find(lPID.begin(), lPID.end(), nPID) == lPID.end()) break;
            // PID 不一样，继续查找
            if (pPID[idxPID].nPID != nPID) continue;

            // PID 一致
            OBD_Mode01Signal *pSig = pPID[idxPID].pOBD_Mode01Signal;
            if (NULL == pSig) continue;

            for (int idxSig = 0; idxSig < pPID->nNumSignal; idxSig++) {
                // 该信号不上传也不保存
                if (!pSig[idxSig].bIsSave && !pSig[idxSig].bIsSend) continue;
#if 1
                uint64_t lRet = 0;
                OBD_ByteOrder nOrder = pSig[idxSig].nByteOrder;
                int nLen = pSig[idxSig].nLen;                                            // 获取信号长度
                int nStartIndex = pSig[idxSig].nStartBit / 8 + (idxDataPIDByte + 1);       // 获取起始位所在字节的索引
                uint8_t u8DataStart = (pMsgData[nStartIndex]);      // 获取起始位所在字节的值
                int nStartOffest = pSig[idxSig].nStartBit % 8;      // 获取起始位在字节中第几位

                // 起始字节
                int nLeft = 0;
                u8DataStart >>= nStartOffest;       // 获取首字节包含的值 （start bit ～ the 8th bit）
                nLeft = nLen - (8 - nStartOffest);  // 获取首字节外剩下的位长度

                // OBD 都是大端序，小端相关代码可去除
                if (nLeft > 0) {                        // 信号跨字节
                    // 中间整字节
                    lRet |= u8DataStart;
                    int nByteCount = nLeft / 8;         // get the number of bytes of the remaining bit length
                    int tempByteCount = nByteCount;

                    uint64_t temp = 0;
                    if (OBD_LITTLE_ENDIAN == nOrder) {
                        /* Intel */
                        while (nByteCount--) {
                            temp = ((uint64_t)pMsgData[++nStartIndex]) << (((tempByteCount - nByteCount) << 3) - nStartOffest);
                            lRet |= temp;
                        } 
                    } else if (OBD_BIG_ENDIAN == nOrder) {
                        /* Motorola */
                        while (nByteCount--) {
                            temp = ((uint64_t)pMsgData[--nStartIndex]) << (((tempByteCount - nByteCount) << 3) - nStartOffest);
                            lRet |= temp;                  
                        }
                    }
                    // 最后剩余位数
                    int nEndLength = nLeft % 8;     // the remaining bit length in addition to the whole byte
                    if (nEndLength) {

                        if (OBD_LITTLE_ENDIAN == nOrder) {
                            uint8_t u8DataEnd = pMsgData[++nStartIndex];
                            u8DataEnd &= (0xFF >> (8 - nEndLength));
                            uint64_t temp = ((uint64_t)u8DataEnd) << (nLen - nEndLength);
                            lRet |= temp;
                        } else if (OBD_BIG_ENDIAN == nOrder) {
                            uint8_t u8DataEnd = pMsgData[--nStartIndex];
                            u8DataEnd &= (0xFF >> (8 - nEndLength));
                            uint64_t temp = ((uint64_t)u8DataEnd) << (nLen - nEndLength);
                            lRet |= temp;
                        }
                    }
                } else {        // the signal is in one byte
                    u8DataStart &= (0xFF >> (8 - nLen));
                    lRet |= u8DataStart;
                }
                
                double dFactor = pSig[idxSig].StcFix.dFactor;
                double dOffset = pSig[idxSig].StcFix.dOffset;
                double dDenominator = pSig[idxSig].StcFix.dDenominator;
                double dFinalOffest = pSig[idxSig].StcFix.dFinalOffest;
                double dPhyValue = SIGNAL_NAN;

                // 有符号整型补全符号位时要注意，除了有效位其余位均需填充(算术右移)
                if (OBD_SIGNED == pSig[idxSig].nType) {
                    uint64_t bSign = (lRet >> (nLen - 1)) & 0x01;  // 符号位
                    if (nLen > 32) {
                        uint64_t mask = (bSign == 1 ? 0xFFFFFFFFFFFFFFFF << nLen : 0);
                        dPhyValue = (((int64_t)(lRet | mask )) * dFactor + dOffset) / dDenominator + dFinalOffest;
                    } else if (nLen > 16) {
                        uint32_t mask = (bSign == 1 ? 0xFFFFFFFF << nLen : 0);
                        dPhyValue = (((int32_t)(lRet | mask )) * dFactor + dOffset) / dDenominator + dFinalOffest;
                    } else if (nLen > 8) {
                        uint16_t mask = (bSign == 1 ? 0xFFFF << nLen : 0);
                        dPhyValue = (((int16_t)(lRet | mask )) * dFactor + dOffset) / dDenominator + dFinalOffest;
                    } else {
                        uint8_t mask = (bSign == 1 ? 0xFF << nLen : 0);
                        dPhyValue = (((int8_t)(lRet | mask )) * dFactor + dOffset) / dDenominator + dFinalOffest;
                    }
                } else if (OBD_UNSIGNED == pSig[idxSig].nType) {
                    dPhyValue = (((uint64_t)lRet) * dFactor + dOffset) / dDenominator + dFinalOffest;
                } else if (OBD_FLOAT == pSig[idxSig].nType) {
                    void *temp = &lRet;
                    dPhyValue = ((*(float *)(temp)) * dFactor + dOffset) / dDenominator + dFinalOffest;
                } else if (OBD_DOUBLE == pSig[idxSig].nType) {
                    void *temp = &lRet;
                    dPhyValue = ((*(double *)(temp)) * dFactor + dOffset) / dDenominator + dFinalOffest;
                }

                Out_Message Msg;
                Msg.strName = pSig[idxSig].strOutName;
                Msg.dPhyVal = dPhyValue;
                Msg.strPhyUnit = pSig[idxSig].strOBDSignalUnit;
                Msg.strPhyFormat = pSig[idxSig].strOBDSignalFormat;
                // 写输出信号 map
                if (pSig[idxSig].bIsSend) {
                    std::lock_guard<std::mutex> lock(g_stu_OutMessage.msg_lock);
                    write_msg_out_map(pSig[idxSig].strOutName, Msg);
                }
                // 写存储信号 map
                if (pSig[idxSig].bIsSave) {
                    Msg.strName = pSig[idxSig].strSaveName;
                    std::lock_guard<std::mutex> lock(g_stu_SaveMessage.msg_lock);
                    write_msg_save_map(pSig[idxSig].strSaveName, Msg);
                }
#endif
#if 0            
                std::cout << "Channel name = " << ChanName << "  ResponseID = " << std::hex << ItrRSP->first << std::endl;
                std::cout << "i = " << idxSig << ", pSig[idxSig].strOutName : " << pSig[idxSig].strOutName;
                std::cout << ", dPhyValue = " << dPhyValue << std::endl << std::endl;
#endif

            }
            // 取得下一个 PID 所在字节
            idxDataPIDByte += (pPID[idxPID].nByteSize + 1);

        }

    }

    return 0;
}

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
        int nLen = pSig[idxSig].nLen;                       // 获取信号长度
        int nStartIndex = pSig[idxSig].nStartBit / 8;       // 获取起始位所在字节的索引
        uint8_t u8DataStart = (pMsgData[nStartIndex]);      // 获取起始位所在字节的值
        int nStartOffest = pSig[idxSig].nStartBit % 8;      // 获取起始位在字节中第几位,get the position of the start bit in the corrdsponding byte

        // 起始字节
        int nLeft = 0;
        if (DBC_Intel == nOrder) {
            u8DataStart >>= nStartOffest;       // 获取首字节包含的值 （start bit ～ the 8th bit）
            nLeft = nLen - (8 - nStartOffest);  // 获取首字节外剩下的位长度
        } else if (DBC_Motorola == nOrder) {
#ifdef Motorola_LSB
            u8DataStart >>= nStartOffest;
            nLeft = nLen - (8 - nStartOffest);
#else
            u8DataStart &= (0xFF >> (7 - nStartOffest));       // get the value from start bit to the 1th bit of the corresponding byte
            nLeft = nLen - (nStartOffest + 1);  // get the remaining bit length after removing the length of nStartOffest
#endif
        }

        if (nLeft > 0) {                        // 信号跨字节
            // 中间整字节
            lRet |= u8DataStart;
            int nByteCount = nLeft / 8;         // get the number of bytes of the remaining bit length
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
            Msg.strName = pSig[idxSig].strSaveName;
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
    CCP_MAP_Slave_DAQList::iterator ItrCCP = (ItrChan->second).CAN.mapCCP.begin();
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
        double dOffset = pEle->StcFix.dOffset;
        double dPhyValue = 0.0;

        if (pEle->nType == CCP_SIGNED) {
            uint64_t bSign = (lRet >> (8 * pEle->nLen - 1)) & 0x01;  // 符号位
            if (pEle->nLen == 1) {
                dPhyValue = (int8_t)(lRet | bSign << 7) * dFactor + dOffset;
            } else if (pEle->nLen == 2) {
                dPhyValue = (int16_t)(lRet | bSign << 15) * dFactor + dOffset;
            } else if (pEle->nLen == 4) {
                dPhyValue = (int32_t)(lRet | bSign << 31) * dFactor + dOffset;
            }
        } else if (pEle->nType == CCP_UNSIGNED) {
            dPhyValue = (uint32_t)lRet * dFactor + dOffset;
        } else if (pEle->nType == CCP_FLOAT) {
            void *temp = &lRet;
            dPhyValue = (*(float *)(temp)) * dFactor + dOffset;
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
            Msg.strName = pEle->strSaveName;
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

int can_xcp_convert2msg(const char *ChanName, uint32_t nCANID, uint8_t *pMsgData) {
    if (NULL == ChanName) {
        return -1;
    }

    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
    if (ItrChan == g_Channel_MAP.end()) {
        XLOG_TRACE("WARN: xcp_convert: channel {} not exist.", ChanName);
        return -1;
    }

    // 检查 ID
    XCP_MAP_Slave_DAQList::iterator ItrXCP = (ItrChan->second).CAN.mapXCP.begin();
    for (; ItrXCP != (ItrChan->second).CAN.mapXCP.end(); ItrXCP++) {
        std::vector<XCP_DAQList>::iterator ItrVecDAQ = (ItrXCP->second).begin();
        for (; ItrVecDAQ != (ItrXCP->second).end(); ItrVecDAQ++) {
            // printf("(*ItrVecDAQ).nCANID = 0x%02X\n", (*ItrVecDAQ).nCANID);
            // printf("nCANID = 0x%02X\n", nCANID);
            if ((*ItrVecDAQ).nCANID != nCANID) continue;
            break;
        }
        if (ItrVecDAQ == (ItrXCP->second).end()) continue;
        break;
    }
    if (ItrXCP == (ItrChan->second).CAN.mapXCP.end()) {
        XLOG_TRACE("WARN: xcp_convert: channel {} dosen't exist DAQList_ID:{}.", ChanName, nCANID);
        return -1;
    }

    // 检查 PID
    std::vector<XCP_DAQList>::iterator ItrVecDAQ = (ItrXCP->second).begin();
    XCP_ODT *pODT = (*ItrVecDAQ).pODT;
    uint8_t nIdxODT = 0;
    uint8_t nPID = pMsgData[0];
    for (; ItrVecDAQ != (ItrXCP->second).end(); ItrVecDAQ++) {
        pODT = (*ItrVecDAQ).pODT;
        nIdxODT = 0;
        for (; nIdxODT < (*ItrVecDAQ).nOdtNum; nIdxODT++) {
            if ((*ItrVecDAQ).nFirstPID + nIdxODT != nPID) continue;
            break;
        }
        if (nIdxODT == (*ItrVecDAQ).nOdtNum) continue;
        break;
    }
    if (ItrVecDAQ == (ItrXCP->second).end()) {
        XLOG_TRACE("WARN: xcp_convert: channel {} DAQList_ID:{} dosen't exist PID {}.", ChanName, nCANID, nPID);
        return -1;
    }
    
    // 数据转换
    XCP_ElementStruct *pEle = pODT[nIdxODT].pElement;
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
                if (pEle->nByteOrder == XCP_LITTLE_ENDIAN) {
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
        double dOffset = pEle->StcFix.dOffset;
        double dPhyValue = 0.0;

        if (pEle->nType == XCP_SIGNED) {
            uint64_t bSign = (lRet >> (8 * pEle->nLen - 1)) & 0x01;  // 符号位
            if (pEle->nLen == 1) {
                dPhyValue = (int8_t)(lRet | bSign << 7) * dFactor + dOffset;
            } else if (pEle->nLen == 2) {
                dPhyValue = (int16_t)(lRet | bSign << 15) * dFactor + dOffset;
            } else if (pEle->nLen == 4) {
                dPhyValue = (int32_t)(lRet | bSign << 31) * dFactor + dOffset;
            }
        } else if (pEle->nType == XCP_UNSIGNED) {
            dPhyValue = (uint32_t)lRet * dFactor + dOffset;
        } else if (pEle->nType == XCP_FLOAT) {
            void *temp = &lRet;
            dPhyValue = (*(float *)(temp)) * dFactor + dOffset;
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
            Msg.strName = pEle->strSaveName;
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
            // debug
            // printf("update |||| save_name: %s, val = %lf", MsgSave.strName, MsgSave.dPhyVal);
            // 修改
            *ItrList = MsgSave;
            break;
        }
    }
    return 0;
}