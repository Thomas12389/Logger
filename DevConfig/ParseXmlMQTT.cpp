
#include <cstring>

#include "DevConfig/ParseXmlMQTT.hpp"
#include "DevConfig/ParseXmlCAN.hpp"
#include "ConvertData/ConvertData.h"
#include "datatype.h"

extern Channel_MAP g_Channel_MAP;
extern Inside_MAP g_Inside_MAP;
extern stu_OutMessage g_stu_OutMessage;
// 需要发布的信号名称
// std::vector<std::string> g_out_MsgName;

int map_sigout2sigin(rapidxml::xml_node<> *pMsgOutNodes, int nNumMessages);

int parse_xml_mqtt(rapidxml::xml_node<> *pMQTTNode, MQTT_Server& MQTT_Ser){
    rapidxml::xml_node<> *pNode = pMQTTNode->first_node("active");
    if (NULL == pNode) {
        return -1;
    }
    char *pActive = pNode->value();
    if (0 == strncmp("true", pActive, strlen("true"))) {
        MQTT_Ser.bIsActive = true;
    } else {
        MQTT_Ser.bIsActive = false;
        return 0;
    }

    pNode = pMQTTNode->first_node("IP");
    MQTT_Ser.strIP = pNode->value();

    pNode = pMQTTNode->first_node("Port");
    MQTT_Ser.nPort = atoi(pNode->value());

    pNode = pMQTTNode->first_node("Username");
    MQTT_Ser.strUserName = pNode->value();

    pNode = pMQTTNode->first_node("Passwd");
    MQTT_Ser.strPasswd = pNode->value();

    pNode = pMQTTNode->first_node("NumModel");
    rapidxml::xml_node<> *pSubNode = pNode->first_node("ReleaseCycle");
    MQTT_Ser.nPublishMs = atoi(pSubNode->value());

    pSubNode = pNode->first_node("NumMessages");
    int m_nNumMessages = atoi(pSubNode->value());

    pNode = pMQTTNode->first_node("Messages");
    if (0 != map_sigout2sigin(pNode, m_nNumMessages)) {
        return -1;
    }

    return 0;
}

int map_sigout2sigin(rapidxml::xml_node<> *pMsgOutNodes, int nNumMessages) {
    if (NULL == pMsgOutNodes) return 0;

    rapidxml::xml_node<> *pNode = NULL;
    rapidxml::xml_node<> *pMsgNode = pMsgOutNodes->first_node("Message");
    rapidxml::xml_node<> *pSubMsgNode = NULL;

    if (NULL == pMsgNode) {
        return -1;
    }

    for (int index = 0; index < nNumMessages; index++) {
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
                pSig[Idx].bIsSend = 1;
                pSig[Idx].strOutName = pMsgNode->first_node("MessageName")->value();
                // printf("pSig[Idx].strOutName : %s, pName : %s\n", pSig[Idx].strOutName.c_str(), pName);
                // 初始化信号值
                g_stu_OutMessage.msg_struct.msg_list.push_back({pSig[Idx].strOutName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
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
// printf("CAN -- 0x%02X <%d>th signal\n", ItrCOMRcv->first, Idx);                   
                    pSig[Idx].bIsSend = 1;
                    pSig[Idx].strOutName = pMsgNode->first_node("MessageName")->value();
                    // 初始化信号值
                    g_stu_OutMessage.msg_struct.msg_list.push_back({pSig[Idx].strOutName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
                    break;
                }
            }
        } else if (0 == strncmp("CCP", pSource, strlen(pSource))) {
            CCP_MAP_Slave_DAQList::iterator ItrCCPRcv = (Itr->second).CAN.mapCCP.begin();
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
                            pEle[nIdxEle].bIsSend = 1;
                            pEle[nIdxEle].strOutName = pMsgNode->first_node("MessageName")->value();
                            // 初始化信号值
                            g_stu_OutMessage.msg_struct.msg_list.push_back({pEle[nIdxEle].strOutName, SIGNAL_NAN, pEle[nIdxEle].strElementUnit, pEle[nIdxEle].strElementFormat});
                            break;
                        }
                    }
                    
                }
            }
        } else if (0 == strncmp("XCP", pSource, strlen(pSource))) {
            XCP_MAP_Slave_DAQList::iterator ItrXCPRcv = (Itr->second).CAN.mapXCP.begin();
            for (; ItrXCPRcv != (Itr->second).CAN.mapXCP.end(); ItrXCPRcv++) {
                std::vector<XCP_DAQList>::iterator ItrDAQList = (ItrXCPRcv->second).begin();
                
                for (; ItrDAQList != (ItrXCPRcv->second).end(); ItrDAQList++) {
                    XCP_ODT *pODT = (*ItrDAQList).pODT;
                    uint8_t nNumODTS = (*ItrDAQList).nOdtNum;
                    uint8_t nIdxODT = 0;

                    for (; nIdxODT < nNumODTS; nIdxODT++) {
                        XCP_ElementStruct *pEle = pODT[nIdxODT].pElement;
                        uint8_t nNumEles = pODT[nIdxODT].nNumElements;
                        uint8_t nIdxEle = 0;

                        for (; nIdxEle < nNumEles; nIdxEle++) {
                            if (pName != pEle[nIdxEle].strElementName) continue;
// printf("XCP -- 0x%02X <%ld>th DAQList <%d>th ODT <%d>th Ele\n", (*ItrDAQList).nCANID, std::distance((ItrXCPRcv->second).begin(),ItrDAQList), nIdxODT, nIdxEle);
                            pEle[nIdxEle].bIsSend = 1;
                            pEle[nIdxEle].strOutName = pMsgNode->first_node("MessageName")->value();
                            // 初始化信号值
                            g_stu_OutMessage.msg_struct.msg_list.push_back({pEle[nIdxEle].strOutName, SIGNAL_NAN, pEle[nIdxEle].strElementUnit, pEle[nIdxEle].strElementFormat});
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
                pSig[Idx].bIsSend = 1;
                pSig[Idx].strOutName = pMsgNode->first_node("MessageName")->value();
                // printf("pSig[Idx].strOutName : %s, pName : %s\n", pSig[Idx].strOutName.c_str(), pName);
                // 初始化信号值
                g_stu_OutMessage.msg_struct.msg_list.push_back({pSig[Idx].strOutName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
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
                pSig[Idx].bIsSend = 1;
                pSig[Idx].strOutName = pMsgNode->first_node("MessageName")->value();
                // printf("pSig[Idx].strOutName : %s, pName : %s\n", pSig[Idx].strOutName.c_str(), pName);
                // 初始化信号值
                g_stu_OutMessage.msg_struct.msg_list.push_back({pSig[Idx].strOutName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
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
                pSig[Idx].bIsSend = 1;
                pSig[Idx].strOutName = pMsgNode->first_node("MessageName")->value();
                // printf("pSig[Idx].strOutName : %s, pName : %s\n", pSig[Idx].strOutName.c_str(), pName);
                // 初始化信号值
                g_stu_OutMessage.msg_struct.msg_list.push_back({pSig[Idx].strOutName, SIGNAL_NAN, pSig[Idx].strSigUnit, pSig[Idx].strSigFormat});
                break;
            }
            
        }
#endif
        pMsgNode = pMsgNode->next_sibling("Message");

    }
#ifdef SELF_OBD
    // 2021.11.04 -- debug
    // g_stu_OutMessage.msg_struct.msg_list.push_back({"EngineCoolantTemperature", SIGNAL_NAN, "°C", ""});
    // g_stu_OutMessage.msg_struct.msg_list.push_back({"EngineRPM", SIGNAL_NAN, "1/min", ""});
    // g_stu_OutMessage.msg_struct.msg_list.push_back({"AbsoluteThrottlePostion", SIGNAL_NAN, "%", ""});
    // g_stu_OutMessage.msg_struct.msg_list.push_back({"AmbientAirTemperature", SIGNAL_NAN, "°C", ""});
#endif

#if 0
    LIST_Message::iterator ItrList = g_stu_OutMessage.msg_struct.msg_list.begin();
    for (; ItrList != g_stu_OutMessage.msg_struct.msg_list.end(); ItrList++) {
        printf("name : %s, val = %lf, uint : %s\n", (*ItrList).strName.c_str(), (*ItrList).dPhyVal, (*ItrList).strPhyUnit.c_str());
    }
    printf("\n");
#endif

    return 0;
}