
#include <unistd.h>
#include <cstring>
#include <inttypes.h>

#include "DevConfig/ParseXmlCAN.hpp"
#include "DevConfig/ParseXmlCAN_DBC.hpp"
#include "DevConfig/ParseXmlCAN_CCP.hpp"
#include "DevConfig/ParseXmlCAN_OBD.hpp"
#include "DevConfig/PhyChannel.hpp"

extern Channel_MAP g_Channel_MAP;

int parse_xml_can(rapidxml::xml_node<> *pCANNode) {
    rapidxml::xml_node<> *pNode = pCANNode->first_node("Channels");
    int m_nNumChannels = atoi(pNode->first_attribute("num")->value());

    if (-1 == parse_xml_can_channel(pNode, m_nNumChannels)) {
        return -1;
    }
    
    return 0;
}

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
            } else if(0 == strcmp("CANFD", pNode->value())){
                g_Channel_MAP[ChanName].CAN.stuBuardRate.nCANBR = atoi(pChannel->first_node("CAN_Rate")->value());
                g_Channel_MAP[ChanName].CAN.stuBuardRate.nCANFDBR = atoi(pChannel->first_node("CANFD_Data_Rate")->value());
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
            // int nSalves = atoi(pNode->first_attribute("num")->value());
            int nSalves = 1;
            parse_xml_ccp_salve(pNode, ChanName, nSalves);
        }

#ifndef SELF_OBD
        pNode = pChannel->first_node("OBD");
        if (NULL == pNode) {
            pNode = pChannel->first_node("WWHOBD");
            if (NULL == pNode) {
                pNode = pChannel->first_node("J1939");
            }
        }
        if (NULL != pNode) {
            int nNumSignal = atoi(pNode->first_attribute("num")->value());
            parse_xml_obd(pNode, ChanName, pNode->name(), nNumSignal);
        }
#else
        // TODO: CAN OBD
        static int num_package = 1; // 从配置读取
        parse_xml_obd(ChanName, num_package);
#endif    
        pChannel = pChannel->next_sibling("Channel");
    }

    return 0;
}

