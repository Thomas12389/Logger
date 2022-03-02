#include "DevConfig/ParseXmlCAN_CCP.hpp"
#include "DevConfig/PhyChannel.hpp"
#include <cstring>

#include "Logger/Logger.h"

extern Channel_MAP g_Channel_MAP;

int parse_xml_ccp_slave(rapidxml::xml_node<> *pCCPNode, const char *ChanName, int nNumSlaves) {

    if (NULL == pCCPNode) return -1;
    rapidxml::xml_node<> *pSlave = pCCPNode->first_node("Slave");
    while (nNumSlaves-- && pSlave) {
        CCP_SlaveData stuCCP_SlaveData;
        // Get slave data
        rapidxml::xml_node<> *pNode = pSlave->first_node("CRO_CAN_ID");
	    stuCCP_SlaveData.nIdCRO = strtoul(pNode->value(), NULL, 16);
  
        pNode = pSlave->first_node("DTO_CAN_ID");
	    stuCCP_SlaveData.nIdDTO = strtoul(pNode->value(), NULL, 16);

        pNode = pSlave->first_node("STATION_ADDRESS");
	    stuCCP_SlaveData.nEcuAddress = strtoul(pNode->value(), NULL, 16);

        pNode = pSlave->first_node("BYTE_ORDER");
        stuCCP_SlaveData.bIntelFormat = atoi(pNode->value()) ? CCP_LITTLE_ENDIAN : CCP_BIG_ENDIAN;
    
        pNode = pSlave->first_node("VERSION");
	    stuCCP_SlaveData.nProVersion = strtol(pNode->value(), NULL, 16);

        pNode = pSlave->first_node("DAQLists");

        while (NULL != pNode) {
            parse_xml_ccp_daq(stuCCP_SlaveData, pNode, ChanName);
            pNode = pNode->next_sibling("DAQLists");
        }
    }

 
    return 0;
}

int parse_xml_ccp_daq(CCP_SlaveData SlaveData, rapidxml::xml_node<> *pDaqListsNode, const char *ChanName) {

    rapidxml::xml_node<> *pDaqList = pDaqListsNode->first_node("DAQList");

    while (pDaqList != NULL) {  
        parse_xml_ccp_odt(SlaveData, pDaqList, ChanName);
        pDaqList = pDaqList->next_sibling("DAQList");
    }
    return 0;
}

int parse_xml_ccp_odt(CCP_SlaveData SlaveData, rapidxml::xml_node<> *pDaqListNode, const char *ChanName) {
    rapidxml::xml_node<> *pNode = NULL;

    pNode = pDaqListNode->first_node("NumODTs");
    int nNumODTs = atoi(pNode->value());

    CCP_DAQList stuCCP_DAQList;
    stuCCP_DAQList.pODT = new CCP_ODT[nNumODTs];
    stuCCP_DAQList.nOdtNum = 0;

    pNode = pDaqListNode->first_node("DAQ_NO");
    stuCCP_DAQList.nDaqNO = atoi(pNode->value());

    pNode = pDaqListNode->first_node("CAN_ID");
    stuCCP_DAQList.nCANID = strtoul(pNode->value(), NULL, 16);

    pNode = pDaqListNode->first_node("EVENT_NO");
    stuCCP_DAQList.nEventChNO = atoi(pNode->value());

    pNode = pDaqListNode->first_node("PRESCALER");
    stuCCP_DAQList.nPrescaler = atoi(pNode->value());

    rapidxml::xml_node<> *pODT = pDaqListNode->first_node("ODT");
    for (int i = 0; i < nNumODTs; i++) {
        if (pODT != NULL) {
        
            parse_xml_ccp_element(pODT, stuCCP_DAQList.pODT + stuCCP_DAQList.nOdtNum);
            stuCCP_DAQList.nOdtNum++;

            pODT = pODT->next_sibling("ODT");
        }
    }

    if (stuCCP_DAQList.nOdtNum) {
        // TODO
        Channel_MAP::iterator Itr = g_Channel_MAP.find(ChanName);
        if (Itr != g_Channel_MAP.end()) {
            (Itr->second).CAN.mapCCP[SlaveData].push_back(stuCCP_DAQList);
        } else {
            CCP_MAP_Slave_DAQList CCPMapSlaveCAQList;
            CCPMapSlaveCAQList[SlaveData].push_back(stuCCP_DAQList);
            g_Channel_MAP[ChanName].CAN.mapCCP = CCPMapSlaveCAQList;
        }

    } else {
        delete[] stuCCP_DAQList.pODT;
    }

    return 0;
}


int parse_xml_ccp_element(rapidxml::xml_node<> *pODTNode, CCP_ODT *pCCP_ODT) {
    rapidxml::xml_node<> *pNode = NULL;
  
    pNode = pODTNode->first_node("NumMeasurements");
    int nNumElements = atoi(pNode->value());
    if (nNumElements > 7) {
        nNumElements = 7;
        XLOG_WARN("The number of Elements exceeds 7!");
    }

    pCCP_ODT->nNumElements = 0;
    pCCP_ODT->pElement = new CCP_ElementStruct[nNumElements];

    pNode = pODTNode->first_node("ODT_NO");
    pCCP_ODT->nOdtNO = atoi(pNode->value());

    rapidxml::xml_node<> *pElement = pODTNode->first_node("MEASUREMENT");
    CCP_ElementStruct *pCCP_Ele = pCCP_ODT->pElement;

    int nEleTotalSize = 0;
    for (int i = 0; i < nNumElements; i++) {
        if (pElement != NULL) {

            pCCP_Ele[pCCP_ODT->nNumElements].bIsSend = 0;
            pCCP_Ele[pCCP_ODT->nNumElements].strOutName = "";
            pCCP_Ele[pCCP_ODT->nNumElements].bIsSend = 0;
            pCCP_Ele[pCCP_ODT->nNumElements].strSaveName = "";

            pNode = pElement->first_node("NAME");
            pCCP_Ele[pCCP_ODT->nNumElements].strElementName = pNode->value();

            pNode = pElement->first_node("Element_NO");
            pCCP_Ele[pCCP_ODT->nNumElements].nEleNO = atoi(pNode->value());

            pNode = pElement->first_node("SIZE");
            nEleTotalSize += atoi(pNode->value());
            if (nEleTotalSize > 7) {
                XLOG_WARN("Element Total size in <%d>th ODT is too large!", pCCP_ODT->nOdtNO);
                break;
            }
            pCCP_Ele[pCCP_ODT->nNumElements].nLen = atoi(pNode->value());
            // TODO：数据类型表示
            pNode = pElement->first_node("TYPE");
            char *pStrType = pNode->value();
            if (strcmp("SIGNED", pStrType) == 0) {
                pCCP_Ele[pCCP_ODT->nNumElements].nType = CCP_SIGNED;
            } else if (strcmp("UNSIGNED", pStrType) == 0) {
                pCCP_Ele[pCCP_ODT->nNumElements].nType = CCP_UNSIGNED;
            } else if (strcmp("FLOAT", pStrType) == 0) {
                pCCP_Ele[pCCP_ODT->nNumElements].nType = CCP_FLOAT;
            }

            pNode = pElement->first_node("BIT_MASK");
            pCCP_Ele[pCCP_ODT->nNumElements].StcFix.cMask = strtoul(pNode->value(), NULL, 16);

            pNode = pElement->first_node("BYTE_ORDER");
            pCCP_Ele[pCCP_ODT->nNumElements].nByteOrder = (CCP_ByteOrder)atoi(pNode->value());

            pNode = pElement->first_node("ECU_ADDRESS");
            pCCP_Ele[pCCP_ODT->nNumElements].nAddress = strtoul(pNode->value(), NULL, 16);

            pNode = pElement->first_node("ADDRESS_EXTENSION");
            pCCP_Ele[pCCP_ODT->nNumElements].nAddress_Ex = strtoul(pNode->value(), NULL, 16);

            pNode = pElement->first_node("GAIN");
            pCCP_Ele[pCCP_ODT->nNumElements].StcFix.dFactor = atof(pNode->value());

            pNode = pElement->first_node("OFFSET");
            pCCP_Ele[pCCP_ODT->nNumElements].StcFix.dOffset = atof(pNode->value());

            pNode = pElement->first_node("FORMAT");
            pCCP_Ele[pCCP_ODT->nNumElements].strElementFormat = pNode->value();

            pNode = pElement->first_node("UNIT");
            pCCP_Ele[pCCP_ODT->nNumElements].strElementUnit = pNode->value();

            pCCP_ODT->nNumElements++;

            pElement = pElement->next_sibling("MEASUREMENT");
        }
    }

    if (!pCCP_ODT->nNumElements) {
        delete[] pCCP_ODT->pElement;
    }

    return 0;
}
