#include "DevConfig/ParseXmlCAN_OBD.hpp"
#include "DevConfig/PhyChannel.hpp"

extern Channel_MAP g_Channel_MAP;

#ifdef SELF_OBD

// mock configure file
static uint32_t nOBDMsgID = 0x7E8;
static uint8_t num_function = 1;
static uint8_t num_subfunc = 4;
static uint32_t obd_signal_functionID{0x01};
static std::vector<uint32_t> obd_signal_subfuncID = {0x05, 0x0C, 0x11, 0x46};
static std::vector<std::string> obd_signal_name = {"EngineCoolantTemperature", "EngineRPM", "AbsoluteThrottlePostion", "AmbientAirTemperature"};
static std::vector<std::string> obd_signal_unit = {"°C", "1/min", "%", "°C"};
static std::vector<std::string> obd_signal_format = {"%d", "%.2f", "%.6f", "%d"};
static std::vector<double> obd_signal_factor = {1.0, 0.25, 0.392157, 1.0};
static std::vector<double> obd_signal_offest = {-40.0, 0.0, 0.0, -40.0};
static std::vector<double> obd_signal_denominator = {1.0, 1.0, 1.0, 1.0};

int parse_xml_obd_subfunc(OBD_Function *pOBD_Function) {
    if (NULL == pOBD_Function) {
        return -1;
    }
    // 从配置读取，当前的 functionID
    int nFunctionID = obd_signal_functionID;
    pOBD_Function->nFunctionID = nFunctionID;
    // 从配置读取，当前 function 的 subfunc 个数
    int nNumSubFunc = num_subfunc;
    if (nNumSubFunc <= 0) return -1;

    pOBD_Function->nNumSubFunction = 0;
    pOBD_Function->pOBD_SubFunc = new OBD_SubFunc[nNumSubFunc];

    OBD_SubFunc *pOBD_SubFunc = pOBD_Function->pOBD_SubFunc;

    for (int i = 0; i < nNumSubFunc; i++) {
        // 初始化
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].bIsSend = 0;
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].strOutName = "";
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].bIsSave = 0;
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].strSaveName = "";
        // subfuncID
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].nSubFuncID = obd_signal_subfuncID[pOBD_Function->nNumSubFunction];
        // 名称和单位
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].strOBDSubFuncName = obd_signal_name[pOBD_Function->nNumSubFunction];
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].strOBDSubFuncUnit = obd_signal_unit[pOBD_Function->nNumSubFunction];
        // 输出格式
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].strOBDSubFuncFormat = obd_signal_format[pOBD_Function->nNumSubFunction];
        // 计算因子
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].StcFix.dFactor = obd_signal_factor[pOBD_Function->nNumSubFunction];
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].StcFix.dOffset = obd_signal_offest[pOBD_Function->nNumSubFunction];
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].StcFix.dDenominator = obd_signal_denominator[pOBD_Function->nNumSubFunction];
        
        // TODO： 向外发布，应该在 MQTT 部分解析
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].bIsSend = 1;
        pOBD_SubFunc[pOBD_Function->nNumSubFunction].strOutName = obd_signal_name[pOBD_Function->nNumSubFunction];
        //
        pOBD_Function->nNumSubFunction++;
    }

    if (!pOBD_Function->nNumSubFunction) {
        delete[] pOBD_Function->pOBD_SubFunc;
    }

    return 0;
}

int parse_xml_obd_function(const char *ChanName) {
    // 从配置读取
    int nNumFunction = num_function;
    if (nNumFunction <= 0) return 0;
    // 从配置读取
    uint32_t nMsgID = nOBDMsgID;

    OBD_RecvPackage *pOBD_RecvPackage = new OBD_RecvPackage;
    pOBD_RecvPackage->pOBD_Function = new OBD_Function[nNumFunction];
    pOBD_RecvPackage->nNumFunction = 0;

    for (int i = 0; i < nNumFunction; i++) {
        parse_xml_obd_subfunc(pOBD_RecvPackage->pOBD_Function + pOBD_RecvPackage->nNumFunction);
        pOBD_RecvPackage->nNumFunction++;
    }

    if (pOBD_RecvPackage->nNumFunction > 0) {
        Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
        if (ItrChan != g_Channel_MAP.end()) {
            (ItrChan->second).CAN.mapOBD[nMsgID] = pOBD_RecvPackage;
        } else {
            OBD_MAP_MsgID_RMsgPkg MapMsgIDRMsgPkg;
            MapMsgIDRMsgPkg[nMsgID] = pOBD_RecvPackage;
            g_Channel_MAP[ChanName].CAN.mapOBD = MapMsgIDRMsgPkg;
        }
    } else {
        delete[] pOBD_RecvPackage->pOBD_Function;
        delete pOBD_RecvPackage;
    }

    return 0;
}


int parse_xml_obd(const char *ChanName, int nNumPacks) {

    for (int i = 0; i < nNumPacks; i++) {
        parse_xml_obd_function(ChanName);
        
    }
    return 0;
}

#else

int parse_xml_obd(rapidxml::xml_node<> *pOBDNode, const char *ChanName, const char *pOBDType, int nNumSignals) {
    if (NULL == pOBDNode) return -1;

    if (nNumSignals <= 0) return 0;

    rapidxml::xml_node<> *pSigNode = NULL;
    OBD_Message *pOBDMessage = new OBD_Message;
    pOBDMessage->pOBDSig = new OBD_SigStruct[nNumSignals];
    pOBDMessage->nNumSigs = 0;

    // pOBDMessage->RequestID = strtoul(pOBDNode->first_attribute("RequestCANID")->value(), NULL, 16);
    // pOBDMessage->ResponseID = strtoul(pOBDNode->first_attribute("ResponseCANID")->value(), NULL, 16);

    pSigNode = pOBDNode->first_node("Signal");

    OBD_SigStruct *pOBDSig = pOBDMessage->pOBDSig;
    while (nNumSignals-- && pSigNode) {    // pNode 为 NULL 时结束，否则配置错误时会出现段错误
        pOBDSig[pOBDMessage->nNumSigs].nIndex = atoi(pSigNode->first_node("IDX")->value());
        pOBDSig[pOBDMessage->nNumSigs].bIsSend = 0;
        pOBDSig[pOBDMessage->nNumSigs].strOutName = "";
        pOBDSig[pOBDMessage->nNumSigs].bIsSave = 0;
        pOBDSig[pOBDMessage->nNumSigs].strSaveName = "";
        pOBDSig[pOBDMessage->nNumSigs].strSigFormat = "";
        pOBDSig[pOBDMessage->nNumSigs].strSigName = pSigNode->first_node("Name")->value();
        pOBDSig[pOBDMessage->nNumSigs].strSigUnit = pSigNode->first_node("Unit")->value();
        pOBDMessage->nNumSigs++;

        pSigNode = pSigNode->next_sibling("Signal");
    }

    if (pOBDMessage->nNumSigs) {
        Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
        if (ItrChan != g_Channel_MAP.end()) {
            (ItrChan->second).CAN.mapOBD[pOBDType] = pOBDMessage;
            // printf("pOBDType = %s\n", pOBDType);
        } else {
            OBD_MAP_Type_Msg MAPTypeMsg;
            MAPTypeMsg[pOBDType] = pOBDMessage;
            g_Channel_MAP[ChanName].CAN.mapOBD = MAPTypeMsg;
            // printf("pOBDType = %s\n", pOBDType);
        }
    } else {
        delete[] pOBDMessage->pOBDSig;
        delete pOBDMessage;
    }

    return 0;
}
#endif

