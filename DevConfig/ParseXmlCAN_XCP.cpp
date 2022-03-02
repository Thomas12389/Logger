#include "DevConfig/ParseXmlCAN_XCP.hpp"
#include "DevConfig/PhyChannel.hpp"
#include <cstring>

#include "Logger/Logger.h"

extern Channel_MAP g_Channel_MAP;


int parse_xml_xcp_element(rapidxml::xml_node<> *pODTNode, XCP_ODT *pXCP_ODT) {
    rapidxml::xml_node<> *pNode = NULL;
    
    pNode = pODTNode->first_node("NumMeasurements");
    int nNumElements = atoi(pNode->value());
    if (nNumElements > 7) {
        nNumElements = 7;
        XLOG_WARN("The number of Elements exceeds 7!");
    }

    pXCP_ODT->nNumElements = 0;
    pXCP_ODT->pElement = new XCP_ElementStruct[nNumElements];

    pNode = pODTNode->first_node("ODT_NO");
    pXCP_ODT->nOdtNO = atoi(pNode->value());

    rapidxml::xml_node<> *pElement = pODTNode->first_node("MEASUREMENT");
    XCP_ElementStruct *pXCP_Ele = pXCP_ODT->pElement;

    int nEleTotalSize = 0;
    for (int i = 0; i < nNumElements; i++) {
        if (pElement != NULL) {

            pXCP_Ele[pXCP_ODT->nNumElements].bIsSend = 0;
            pXCP_Ele[pXCP_ODT->nNumElements].strOutName = "";
            pXCP_Ele[pXCP_ODT->nNumElements].bIsSend = 0;
            pXCP_Ele[pXCP_ODT->nNumElements].strSaveName = "";

            pNode = pElement->first_node("NAME");
            pXCP_Ele[pXCP_ODT->nNumElements].strElementName = pNode->value();

            pNode = pElement->first_node("Element_NO");
            pXCP_Ele[pXCP_ODT->nNumElements].nEleNO = atoi(pNode->value());

            pNode = pElement->first_node("SIZE");
            nEleTotalSize += atoi(pNode->value());
            if (nEleTotalSize > 7) {
                XLOG_WARN("Element Total size in <%d>th ODT is too large!", pXCP_ODT->nOdtNO);
                break;
            }
            pXCP_Ele[pXCP_ODT->nNumElements].nLen = atoi(pNode->value());
            // TODO：数据类型表示
            pNode = pElement->first_node("TYPE");
            char *pStrType = pNode->value();
            if (strcmp("SIGNED", pStrType) == 0) {
                pXCP_Ele[pXCP_ODT->nNumElements].nType = XCP_SIGNED;
            } else if (strcmp("UNSIGNED", pStrType) == 0) {
                pXCP_Ele[pXCP_ODT->nNumElements].nType = XCP_UNSIGNED;
            } else if (strcmp("FLOAT", pStrType) == 0) {
                pXCP_Ele[pXCP_ODT->nNumElements].nType = XCP_FLOAT;
            }

            pNode = pElement->first_node("BIT_MASK");
            pXCP_Ele[pXCP_ODT->nNumElements].StcFix.cMask = strtoul(pNode->value(), NULL, 16);

            pNode = pElement->first_node("BYTE_ORDER");
            pXCP_Ele[pXCP_ODT->nNumElements].nByteOrder = (XCP_ByteOrder)atoi(pNode->value());

            pNode = pElement->first_node("ECU_ADDRESS");
            pXCP_Ele[pXCP_ODT->nNumElements].nAddress = strtoul(pNode->value(), NULL, 16);

            pNode = pElement->first_node("ADDRESS_EXTENSION");
            pXCP_Ele[pXCP_ODT->nNumElements].nAddress_Ex = strtoul(pNode->value(), NULL, 16);

            pNode = pElement->first_node("GAIN");
            pXCP_Ele[pXCP_ODT->nNumElements].StcFix.dFactor = atof(pNode->value());

            pNode = pElement->first_node("OFFSET");
            pXCP_Ele[pXCP_ODT->nNumElements].StcFix.dOffset = atof(pNode->value());

            pNode = pElement->first_node("FORMAT");
            pXCP_Ele[pXCP_ODT->nNumElements].strElementFormat = pNode->value();

            pNode = pElement->first_node("UNIT");
            pXCP_Ele[pXCP_ODT->nNumElements].strElementUnit = pNode->value();

            pXCP_ODT->nNumElements++;

            pElement = pElement->next_sibling("MEASUREMENT");
        }
    }

    if (!pXCP_ODT->nNumElements) {
        delete[] pXCP_ODT->pElement;
    }

    return 0;
}

int parse_xml_xcp_odt(XCP_SlaveData SlaveData, rapidxml::xml_node<> *pDaqListNode, const char *ChanName) {
    rapidxml::xml_node<> *pNode = NULL;

    pNode = pDaqListNode->first_node("NumODTs");
    int nNumODTs = atoi(pNode->value());

    XCP_DAQList stuXCP_DAQList;
    stuXCP_DAQList.pODT = new XCP_ODT[nNumODTs];
    stuXCP_DAQList.nOdtNum = 0;

    pNode = pDaqListNode->first_node("DAQ_NO");
    stuXCP_DAQList.nDaqNO = atoi(pNode->value());

    pNode = pDaqListNode->first_node("CAN_ID");
    stuXCP_DAQList.nCANID = strtoul(pNode->value(), NULL, 16);

    pNode = pDaqListNode->first_node("EVENT_NO");
    stuXCP_DAQList.nEventChNO = atoi(pNode->value());

    pNode = pDaqListNode->first_node("PRESCALER");
    stuXCP_DAQList.nPrescaler = atoi(pNode->value());

    rapidxml::xml_node<> *pODT = pDaqListNode->first_node("ODT");
    for (int i = 0; i < nNumODTs; i++) {
        if (pODT != NULL) {
        
            parse_xml_xcp_element(pODT, stuXCP_DAQList.pODT + stuXCP_DAQList.nOdtNum);
            stuXCP_DAQList.nOdtNum++;

            pODT = pODT->next_sibling("ODT");
        }
    }

    if (stuXCP_DAQList.nOdtNum) {
        // TODO
        Channel_MAP::iterator Itr = g_Channel_MAP.find(ChanName);
        if (Itr != g_Channel_MAP.end()) {
            (Itr->second).CAN.mapXCP[SlaveData].push_back(stuXCP_DAQList);
        } else {
            XCP_MAP_Slave_DAQList XCPMapSlaveCAQList;
            XCPMapSlaveCAQList[SlaveData].push_back(stuXCP_DAQList);
            g_Channel_MAP[ChanName].CAN.mapXCP = XCPMapSlaveCAQList;
        }

    } else {
        delete[] stuXCP_DAQList.pODT;
    }

    return 0;
}

int parse_xml_xcp_daq(XCP_SlaveData SlaveData, rapidxml::xml_node<> *pDaqListsNode, const char *ChanName) {

    rapidxml::xml_node<> *pDaqList = pDaqListsNode->first_node("DAQList");

    while (pDaqList != NULL) {  
        parse_xml_xcp_odt(SlaveData, pDaqList, ChanName);
        pDaqList = pDaqList->next_sibling("DAQList");
    }
    return 0;
}


int parse_xml_xcp_slave(rapidxml::xml_node<> *pXCPNode, const char *ChanName, int nNumSlaves) {
    if (NULL == pXCPNode) return -1;

    rapidxml::xml_node<> *pSlave = pXCPNode->first_node("Slave");
    while (nNumSlaves-- && pSlave) {
        XCP_SlaveData stuXCP_SlaveData;
        // Get slave data
        rapidxml::xml_node<> *pNode = pSlave->first_node("CAN_ID_Master");
	    stuXCP_SlaveData.nMasterID = strtoul(pNode->value(), NULL, 16);
  
        pNode = pSlave->first_node("CAN_ID_Slave");
	    stuXCP_SlaveData.nSlaveID = strtoul(pNode->value(), NULL, 16);

        pNode = pSlave->first_node("MAX_DLC_Required");
        char *pMaxDLCRequired = pNode->value();
        if (strcmp("true", pMaxDLCRequired) == 0) {
            stuXCP_SlaveData.bMaxDLCRequired = true;
        } else {
            stuXCP_SlaveData.bMaxDLCRequired = false;
        }

        pNode = pSlave->first_node("BYTE_ORDER");
        stuXCP_SlaveData.bIntelFormat = atoi(pNode->value()) ? XCP_LITTLE_ENDIAN : XCP_BIG_ENDIAN;
    
        pNode = pSlave->first_node("VERSION");
	    stuXCP_SlaveData.nProVersion = strtol(pNode->value(), NULL, 16);

        pNode = pSlave->first_node("DAQLists");

        while (NULL != pNode) {
            parse_xml_xcp_daq(stuXCP_SlaveData, pNode, ChanName);
            pNode = pNode->next_sibling("DAQLists");
        }

    }
 
    return 0;
}
