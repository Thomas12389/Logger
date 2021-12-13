
#include <cstring>

#include "DevConfig/ParseXmlSTORAGE.hpp"
#include "DevConfig/ParseXmlCAN.hpp"
#include "ConvertData/ConvertData.h"
#include "datatype.h"

extern Channel_MAP g_Channel_MAP;
extern Inside_MAP g_Inside_MAP;
extern stu_SaveMessage g_stu_SaveMessage;
// 需要保存到文件的信号名称
// std::vector<std::string> g_save_MsgName;

int map_sigsave2sigin(rapidxml::xml_node<> *pMsgSaveNodes, int nNumMessages);

int parse_xml_storage(rapidxml::xml_node<> *pStorageNode, File_Save& File_Info){

    rapidxml::xml_node<> *pNode = pStorageNode->first_node("FileName");
    File_Info.strRemoteFileName = pNode->value();
    
    pNode = pStorageNode->first_node("NumModel");
    rapidxml::xml_node<> *pSubNode = pNode->first_node("ReleaseCycle");
    File_Info.nSaveCycleMs = atoi(pSubNode->value());

    pSubNode = pNode->first_node("NumMessages");
    int m_nNumMessages = atoi(pSubNode->value());

    pNode = pStorageNode->first_node("Messages");
    if (0 != map_sigsave2sigin(pNode, m_nNumMessages)) {
        return -1;
    }

    return 0;
}

int map_sigsave2sigin(rapidxml::xml_node<> *pMsgSaveNodes, int nNumMessages) {
    if (NULL == pMsgSaveNodes) return 0;

    rapidxml::xml_node<> *pNode = NULL;
    rapidxml::xml_node<> *pMsgNode = pMsgSaveNodes->first_node("Message");
    rapidxml::xml_node<> *pSubMsgNode = NULL;

    if (NULL == pMsgNode) {
        return -1;
    }

    for (int i = 0; i < nNumMessages; i++) {
        if (NULL == pMsgNode) break;    // 防止输出 xml 中 nNumMessages 配置错误（比实际多）造成段错误 

        pSubMsgNode = pMsgNode->first_node("Reference");

        // 1. 检查信号源（如： COM、CCP、GPS 等）
        // 2. 匹配信号
        char *pSource = pSubMsgNode->first_node("Source")->value();
        char *pName = pSubMsgNode->first_node("Name")->value();
        if (0 == strncmp("GPS", pSource, strlen(pSource)) ||
            0 == strncmp("Internal", pSource, strlen(pSource)) ) {

            Inside_MAP::iterator Itr = g_Inside_MAP.find(pSource);
            if (Itr == g_Inside_MAP.end()) {
                pMsgNode = pMsgNode->next_sibling("Message");
                continue;
            }

            Inside_SigStruct *pSig = (Itr->second).pInsideSig;
            for (int Idx = 0; Idx < (Itr->second).nNumSigs; Idx++) {
                // 根据信号名称查找
                if (pName != pSig[Idx].strSigName) continue;
                // 找到对应信号
                pSig[Idx].bIsSave = 1;
                pSig[Idx].strSaveName = pMsgNode->first_node("MessageName")->value();
                // 初始化信号值
                g_stu_SaveMessage.msg_struct.msg_list.push_back({pSig[Idx].strSaveName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
                break;
            }
        }

        // 检查 Channel（如：是 CAN1 或者 CAN2）
        pNode = pSubMsgNode->first_node("Channel");
        // 内置信号无 Channel 节点，如 GPS 或者 Internal 等, 
        if (NULL == pNode) {
            pMsgNode = pMsgNode->next_sibling("Message");
            continue;
        }
        char *ChanName = pNode->value();

        Channel_MAP::iterator Itr = g_Channel_MAP.find(ChanName);
        if (Itr == g_Channel_MAP.end()) {
            pMsgNode = pMsgNode->next_sibling("Message");
            continue;
        }

        // 1. 检查信号源（如：是 CAN1 的 COM 还是 CCP）
        // 2. 匹配信号
        if (0 == strncmp("COM", pSource, strlen(pSource))) {
            COM_MAP_MsgID_RMsgPkg::iterator ItrCOMRcv = (Itr->second).CAN.mapDBC.begin();
            for (; ItrCOMRcv != (Itr->second).CAN.mapDBC.end(); ItrCOMRcv++) {
                DBC_SigStruct *pSig = (ItrCOMRcv->second)->pPackSig;

                for (int Idx = 0; Idx < (ItrCOMRcv->second)->nNumSigs; Idx++) {
                    // 根据信号名比对，不同则继续比对下一个
                    if (pName != pSig[Idx].strSigName) continue;
                    // 找到了对应的输入信号
// printf("DBC -- 0x%02X <%d>th signal\n", ItrCOMRcv->first, Idx);                   
                    pSig[Idx].bIsSave = 1;
                    pSig[Idx].strSaveName = pMsgNode->first_node("MessageName")->value();
                    // 按顺序保存需要存储的信号名称
                    // g_save_MsgName.emplace_back(pSig[Idx].strSaveName);
                    // 初始化信号值
                    g_stu_SaveMessage.msg_struct.msg_list.push_back({pSig[Idx].strSaveName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
                    break;
                }
            }
        } else if (0 == strncmp("CCP", pSource, strlen(pSource))) {
            CCP_MAP_Salve_DAQList::iterator ItrCCPRcv = (Itr->second).CAN.mapCCP.begin();
            for (; ItrCCPRcv != (Itr->second).CAN.mapCCP.end(); ItrCCPRcv++) {
                std::vector<CCP_DAQList>::iterator ItrDAQList = (ItrCCPRcv->second).begin();
                
                for (; ItrDAQList != (ItrCCPRcv->second).end(); ItrDAQList++) {
                    CCP_ODT *pODT = (*ItrDAQList).pODT;
                    uint8_t nNumODTS = (*ItrDAQList).nOdtNum;
                    uint8_t nIdxODT = 0;

                    for (; nIdxODT < nNumODTS; nIdxODT++) {
                        CCP_ElementStruct *pEle = pODT[nIdxODT].pElement;
                        uint8_t nNumEles = pODT[nIdxODT].nNumElements;
                        uint8_t nIdxEle = 0;

                        for (; nIdxEle < nNumEles; nIdxEle++) {
                            if (pName != pEle[nIdxEle].strElementName) continue;
// printf("CCP -- 0x%02X <%ld>th DAQList <%d>th ODT <%d>th Ele\n", (*ItrDAQList).nCANID, std::distance((ItrCCPRcv->second).begin(),ItrDAQList), nIdxODT, nIdxEle);
                            pEle[nIdxEle].bIsSave = 1;
                            pEle[nIdxEle].strSaveName = pMsgNode->first_node("MessageName")->value();
                            // 按顺序保存需要存储的信号名称
                            // g_save_MsgName.emplace_back(pEle[nIdxEle].strSaveName);
                            // 初始化信号值
                            g_stu_SaveMessage.msg_struct.msg_list.push_back({pEle[nIdxEle].strSaveName, SIGNAL_NAN, pEle[nIdxEle].strElementUnit, pEle[nIdxEle].strElementFormat});
                            break;
                        }
                    }
                    
                }
            }
        } 
#ifndef SELF_OBD
        else if (0 == strncmp("OBD", pSource, strlen(pSource))) {
            OBD_MAP_Type_Msg::iterator ItrOBD = (Itr->second).CAN.mapOBD.find("OBD");
            // 不存在 OBD 信号
            if (ItrOBD == (Itr->second).CAN.mapOBD.end()) {
                pMsgNode = pMsgNode->next_sibling("Message");
                continue;
            }

            OBD_SigStruct *pSig = (ItrOBD->second)->pOBDSig;
            for (int Idx = 0; Idx < (ItrOBD->second)->nNumSigs; Idx++) {
                // 根据信号名称查找
                if (pName != pSig[Idx].strSigName) continue;
                // 找到对应信号
                pSig[Idx].bIsSave = 1;
                pSig[Idx].strSaveName = pMsgNode->first_node("MessageName")->value();
                // printf("pSig[Idx].strSaveName : %s, pName : %s\n", pSig[Idx].strSaveName.c_str(), pName);
                // 初始化信号值
                g_stu_SaveMessage.msg_struct.msg_list.push_back({pSig[Idx].strSaveName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
                break;
            }

        } else if (0 == strncmp("WWHOBD", pSource, strlen(pSource))) {
            OBD_MAP_Type_Msg::iterator ItrOBD = (Itr->second).CAN.mapOBD.find("WWHOBD");
            // 不存在 WWHOBD 信号
            if (ItrOBD == (Itr->second).CAN.mapOBD.end()) {
                pMsgNode = pMsgNode->next_sibling("Message");
                continue;
            }

            OBD_SigStruct *pSig = (ItrOBD->second)->pOBDSig;
            for (int Idx = 0; Idx < (ItrOBD->second)->nNumSigs; Idx++) {
                // 根据信号名称查找
                if (pName != pSig[Idx].strSigName) continue;
                // 找到对应信号
                pSig[Idx].bIsSave = 1;
                pSig[Idx].strSaveName = pMsgNode->first_node("MessageName")->value();
                // printf("pSig[Idx].strSaveName : %s, pName : %s\n", pSig[Idx].strSaveName.c_str(), pName);
                // 初始化信号值
                g_stu_SaveMessage.msg_struct.msg_list.push_back({pSig[Idx].strSaveName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
                break;
            }

        } else if (0 == strncmp("J1939", pSource, strlen(pSource))) {
            OBD_MAP_Type_Msg::iterator ItrOBD = (Itr->second).CAN.mapOBD.find("J1939");
            // 不存在 J1939 信号
            if (ItrOBD == (Itr->second).CAN.mapOBD.end()) {
                pMsgNode = pMsgNode->next_sibling("Message");
                continue;
            }

            OBD_SigStruct *pSig = (ItrOBD->second)->pOBDSig;
            for (int Idx = 0; Idx < (ItrOBD->second)->nNumSigs; Idx++) {
                // 根据信号名称查找
                if (pName != pSig[Idx].strSigName) continue;
                // 找到对应信号
                pSig[Idx].bIsSave = 1;
                pSig[Idx].strSaveName = pMsgNode->first_node("MessageName")->value();
                // printf("pSig[Idx].strSaveName : %s, pName : %s\n", pSig[Idx].strSaveName.c_str(), pName);
                // 初始化信号值
                g_stu_SaveMessage.msg_struct.msg_list.push_back({pSig[Idx].strSaveName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
                break;
            }

        }
#endif

        pMsgNode = pMsgNode->next_sibling("Message");

    }

#if 0
    LIST_Message::iterator ItrList = g_stu_SaveMessage.msg_struct.msg_list.begin();
    for (; ItrList != g_stu_SaveMessage.msg_struct.msg_list.end(); ItrList++) {
        printf("name : %s, val = %lf, uint : %s\n", (*ItrList).strName.c_str(), (*ItrList).dPhyVal, (*ItrList).strPhyUnit.c_str());
    }
    printf("\n");
#endif

    return 0;
}