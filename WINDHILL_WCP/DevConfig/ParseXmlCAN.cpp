
#include <unistd.h>
#include <cstring>
#include <inttypes.h>

#include "DevConfig/ParseXmlCAN.hpp"
#include "DevConfig/ParseXmlCAN_DBC.hpp"
#include "DevConfig/ParseXmlCAN_CCP.hpp"
#include "DevConfig/ParseXmlCAN_XCP.hpp"
#include "DevConfig/ParseXmlCAN_OBD.hpp"
#include "DevConfig/PhyChannel.hpp"

extern Channel_MAP g_Channel_MAP;

int parse_xml_can_channel(rapidxml::xml_node<> *pChannelNodes, int nNumChannels) {
    if (NULL == pChannelNodes) return 0;
 
    rapidxml::xml_node<> *pNode = NULL;
    rapidxml::xml_node<> *pChannel = pChannelNodes->first_node("Channel");
    for (int Idx = 0; Idx < nNumChannels; Idx++) {
        if (NULL == pChannel) break;        // 防止 xml 中 m_nNumChannels 配置错误（比实际多）导致段错误

        char *ChanName = pChannel->first_attribute("name")->value();
        pNode  = pChannel->first_node("ChannelType");
        if (NULL != pNode) {
            if (0 == strcmp("CAN", pNode->value())) {
                g_Channel_MAP[ChanName].CAN.stuBuardRate.nCANBR = atoi(pChannel->first_node("CAN_Rate")->value());
                g_Channel_MAP[ChanName].CAN.stuBuardRate.nCANFDBR = 0;
            } else if(0 == strcmp("CANFD", pNode->value())){
                g_Channel_MAP[ChanName].CAN.stuBuardRate.nCANBR = atoi(pChannel->first_node("CAN_Rate")->value());
                g_Channel_MAP[ChanName].CAN.stuBuardRate.nCANFDBR = atoi(pChannel->first_node("CANFD_Data_Rate")->value());
            }
        }

        // 2022.06.20
        pNode = pChannel->first_node("enableTraffic");
        g_Channel_MAP[ChanName].CAN.enableTraffic = false;
        if (NULL != pNode) {
            char *pEnableTraffic = pNode->value();
            if (0 == strncmp("true", pEnableTraffic, strlen("true")) ) {
                g_Channel_MAP[ChanName].CAN.enableTraffic = true;
            }
        }
        // 2022.06.20
        g_Channel_MAP[ChanName].CAN.enableHWFilter = false;
        // 若使能了 traffic 功能，则不配置过滤功能
        if (false == g_Channel_MAP[ChanName].CAN.enableTraffic) {
            // 2022.03.22
            pNode = pChannel->first_node("enableHWFilter");
            if (NULL != pNode) {
                char *pEnableHWFilter = pNode->value();
                if (0 == strncmp("true", pEnableHWFilter, strlen("true")) ) {
                    g_Channel_MAP[ChanName].CAN.enableHWFilter = true;
                }
            }
        }
  
        pNode = pChannel->first_node("COM");
        if (NULL != pNode) {
            int nPacks = atoi(pNode->first_attribute("num")->value());
            parse_xml_dbc_package(pNode, ChanName, nPacks);
        }

        pNode = pChannel->first_node("CCP");
        if (NULL != pNode) {
            // 目前只支持一个
            // int nSlaves = atoi(pNode->first_attribute("num")->value());
            int nSlaves = 1;
            parse_xml_ccp_slave(pNode, ChanName, nSlaves);
        }
   
        pNode = pChannel->first_node("XCP");
        if (NULL != pNode) {
            // 目前只支持一个
            // int nSlaves = atoi(pNode->first_attribute("num")->value());
            int nSlaves = 1;
            parse_xml_xcp_slave(pNode, ChanName, nSlaves);
        }

        // 2022.07.07
        pNode = pChannel->first_node("OBD");
        if (NULL != pNode) {
            int nPacks = atoi(pNode->first_attribute("num")->value());
            parse_xml_obd_package(pNode, ChanName, nPacks);
        }

// #ifndef SELF_OBD
//         pNode = pChannel->first_node("OBD");
//         if (NULL == pNode) {
//             pNode = pChannel->first_node("WWHOBD");
//             if (NULL == pNode) {
//                 pNode = pChannel->first_node("J1939");
//             }
//         }
//         if (NULL != pNode) {
//             int nNumSignal = atoi(pNode->first_attribute("num")->value());
//             parse_xml_obd(pNode, ChanName, pNode->name(), nNumSignal);
//         }
// #else
//         // TODO: CAN OBD
//         static int num_package = 1; // 从配置读取
//         parse_xml_obd(ChanName, num_package);
// #endif    
        pChannel = pChannel->next_sibling("Channel");
    }

    return 0;
}


int parse_xml_can(rapidxml::xml_node<> *pCANNode) {
    rapidxml::xml_node<> *pNode = pCANNode->first_node("Channels");
    int m_nNumChannels = atoi(pNode->first_attribute("num")->value());

    if (-1 == parse_xml_can_channel(pNode, m_nNumChannels)) {
        return -1;
    }
    
    return 0;
}
