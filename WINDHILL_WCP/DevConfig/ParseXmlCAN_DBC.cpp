
#include <cstring>
#include "DevConfig/ParseXmlCAN_DBC.hpp"
#include "DevConfig/PhyChannel.hpp"

extern Channel_MAP g_Channel_MAP;

int parse_xml_dbc_package(rapidxml::xml_node<> *pCOMNode, const char *ChanName, int nNumpkgs) {
    if (NULL == pCOMNode) return -1;

    rapidxml::xml_node<> *pNode = NULL;

    if (nNumpkgs) {
        // parse Packages
        pNode = pCOMNode->first_node("Package");
        while (nNumpkgs-- && pNode) {
            parse_xml_dbc_signal(pNode, ChanName);
            pNode = pNode->next_sibling("Package");
        }
    }

    return 0;
}

int parse_xml_dbc_signal(rapidxml::xml_node<> *pPackageNode, const char *ChanName) {
    if (NULL == pPackageNode) return 0;

    // get number of signal
    char *pStr = pPackageNode->first_node("NumSignals")->value();
    int nNumSignals = atoi(pStr);
    if (nNumSignals <= 0) return 0;

    char *pStrMsgID = pPackageNode->first_node("ID")->value();
    uint32_t nMsgID = strtoul(pStrMsgID, NULL, 16);
    // printf("nMsgID = %x\n", nMsgID);

    ReceiveMessagePack *pReceiveMsg = new ReceiveMessagePack;
    pReceiveMsg->pPackSig = new DBC_SigStruct[nNumSignals];
    pReceiveMsg->nNumSigs = 0;
    // pReceiveMsg->nChannelIndex = nChannelIndex;

    if (nMsgID & 0x80000000) {
        pReceiveMsg->e_Frame_Type = E_FRAME_EXT;
    } else {
        pReceiveMsg->e_Frame_Type = E_FRAME_STD;
    }
    // nMsgID &= 0x1FFFFFFF;
    // printf("nMsgID = %x\n", nMsgID);
    pReceiveMsg->nDataLen = atoi(pPackageNode->first_node("Datalength")->value());

    // parse signal node
    rapidxml::xml_node<> *pSignalsNode = pPackageNode->first_node("Signals");
    rapidxml::xml_node<> *pSignalNode = pSignalsNode->first_node("Signal");
    DBC_SigStruct *pSig = pReceiveMsg->	pPackSig;	

    for (int i = 0; i < nNumSignals; i++) {
        if (NULL != pSignalNode) {
            pSig[pReceiveMsg->nNumSigs].bIsSend = 0;
            pSig[pReceiveMsg->nNumSigs].strOutName = "";
            pSig[pReceiveMsg->nNumSigs].bIsSave = 0;
            pSig[pReceiveMsg->nNumSigs].strSaveName = "";
            pSig[pReceiveMsg->nNumSigs].strSigFormat = "";  // TODO
            pSig[pReceiveMsg->nNumSigs].strSigName = pSignalNode->first_node("Name")->value();
            pSig[pReceiveMsg->nNumSigs].strSigUnit = pSignalNode->first_node("Unit")->value();
            pSig[pReceiveMsg->nNumSigs].nStartBit = atoi(pSignalNode->first_node("StartBit")->value());
            pSig[pReceiveMsg->nNumSigs].nLen = atoi(pSignalNode->first_node("Length")->value());

            // 若未配置 byteorder，默认为 Intel
            rapidxml::xml_node<> *pOrder = pSignalNode->first_node("ByteOrder");
            pSig[pReceiveMsg->nNumSigs].nByteOrder = (DBC_ByteOrder)(pOrder ? atoi(pOrder->value()) : 1);

            char *pStrType = pSignalNode->first_node("Type")->value();
            if (strcmp("SIGNED", pStrType) == 0) {
                pSig[pReceiveMsg->nNumSigs].nType = DBC_SIGNED;
            } else if (strcmp("UNSIGNED", pStrType) == 0) {
                pSig[pReceiveMsg->nNumSigs].nType = DBC_UNSIGNED;
            } else if (strcmp("FLOAT", pStrType) == 0) {
                pSig[pReceiveMsg->nNumSigs].nType = DBC_FLOAT;
            } else if (strcmp("DOUBLE", pStrType) == 0) {
                pSig[pReceiveMsg->nNumSigs].nType = DBC_DOUBLE;
            }

            pSig[pReceiveMsg->nNumSigs].StcFix.dFactor = atof(pSignalNode->first_node("SignalFactor")->value());
            pSig[pReceiveMsg->nNumSigs].StcFix.dOffset = atof(pSignalNode->first_node("SignalOffset")->value());
            pReceiveMsg->nNumSigs++;
            
            pSignalNode = pSignalNode->next_sibling("Signal");
        }
    }
// std::cout << "pReceiveMsg->nNumSigs = " << pReceiveMsg->nNumSigs << std::endl;
    if (pReceiveMsg->nNumSigs) {
        Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
        if (ItrChan != g_Channel_MAP.end()) {
            (ItrChan->second).CAN.mapDBC[nMsgID] = pReceiveMsg;
            // printf("nMsgID = %x\n", nMsgID);
        } else {
            COM_MAP_MsgID_RMsgPkg MapMsgIDRMsgPkg;
            MapMsgIDRMsgPkg[nMsgID] = pReceiveMsg;
            g_Channel_MAP[ChanName].CAN.mapDBC = MapMsgIDRMsgPkg;
            // printf("nMsgID = %x\n", nMsgID);
        }
    } else {
        delete [] pReceiveMsg->pPackSig;
        delete pReceiveMsg;
    }

    return 0;
}

