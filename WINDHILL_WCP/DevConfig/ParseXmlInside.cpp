
#include <unistd.h>
#include <cstring>
#include <inttypes.h>

#include "DevConfig/ParseXmlInside.hpp"
#include "DevConfig/PhyChannel.hpp"

extern Inside_MAP g_Inside_MAP;

int parse_xml_inside_signals(rapidxml::xml_node<> *pGPSNode, const char *pSource, int nNumSignals) {

    rapidxml::xml_node<> *pSignalNode = pGPSNode->first_node("signal");
    
    Inside_SigStruct *pSig = new Inside_SigStruct[nNumSignals];
    int nIdxSig = 0;
    for (; nIdxSig < nNumSignals; nIdxSig++) {
        if (NULL == pSignalNode)  {
            break;
        }

        pSig[nIdxSig].bIsSend = 0;
        pSig[nIdxSig].strOutName = "";
        pSig[nIdxSig].bIsSave = 0;
        pSig[nIdxSig].strSaveName = "";
        pSig[nIdxSig].strSigName = pSignalNode->first_node("Name")->value();
        pSig[nIdxSig].strSigUnit = pSignalNode->first_node("Unit")->value();
        pSig[nIdxSig].strSigFormat = "";  // TODO

        pSignalNode = pSignalNode->next_sibling("signal");
    }

    if (nIdxSig) {
        Inside_MAP::iterator ItrInside = g_Inside_MAP.find(pSource);
        if (ItrInside != g_Inside_MAP.end()) {
            (ItrInside->second).pInsideSig = pSig;
        } else {
            g_Inside_MAP[pSource].pInsideSig = pSig;
        }
        // 记录信号个数
        g_Inside_MAP[pSource].nNumSigs = nIdxSig;
        // printf("g_Inside_MAP[%s].nNumSigs = %d\n", pSource, nIdxSig);
    } else {
        delete [] pSig;
    }

    return 0;
}

int parse_xml_inside(rapidxml::xml_node<> *pInsideNode, const char *pSource) {
    if (NULL == pInsideNode) {
        return 0;
    }
    
    rapidxml::xml_node<> *pNode = pInsideNode->first_node("active");
    char *pStrActive = pNode->value();
    if (0 == strncmp("true", pStrActive, strlen("true"))) {
        g_Inside_MAP[pSource].isEnable = 1;
        g_Inside_MAP[pSource].nSampleCycleMs = 1000 / strtoul(pInsideNode->first_node("sampleRate")->value(), NULL, 10);
        int nNumSignals = atoi(pInsideNode->first_node("numSignals")->value());
        parse_xml_inside_signals(pInsideNode, pSource, nNumSignals);
    } else {
        g_Inside_MAP[pSource].isEnable = 0;
    }

    return 0;
}
