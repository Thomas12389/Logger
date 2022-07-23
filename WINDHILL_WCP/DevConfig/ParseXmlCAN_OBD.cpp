
#include <cstring>
#include "DevConfig/ParseXmlCAN_OBD.hpp"
#include "DevConfig/PhyChannel.hpp"

extern Channel_MAP g_Channel_MAP;

// mock configure file
/*
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
*/

int parse_xml_obd_signal(rapidxml::xml_node<> *pPIDNode, OBD_Mode01PID *pOBD_Mode01PID) {

    // 没有 Signals 节点，返回
    rapidxml::xml_node<> *pSignalsNode = pPIDNode->first_node("Signals");
    if (NULL == pSignalsNode) return -1;

    // Signals 数量异常，返回
    int nNumSignals = atoi(pSignalsNode->first_attribute("num")->value());
    if (nNumSignals <= 0) return -1;

    pOBD_Mode01PID->pOBD_Mode01Signal = new (std::nothrow)OBD_Mode01Signal[nNumSignals];
    pOBD_Mode01PID->nNumSignal = 0;
    pOBD_Mode01PID->nPID = strtoul(pPIDNode->first_attribute("id")->value(), NULL, 16);
    pOBD_Mode01PID->nByteSize = strtoul(pPIDNode->first_attribute("size")->value(), NULL, 10);
    if (NULL == pOBD_Mode01PID->pOBD_Mode01Signal) return -1;
    // 解析 signal 节点
    OBD_Mode01Signal *pOBD_Mode01Signal = pOBD_Mode01PID->pOBD_Mode01Signal;
    rapidxml::xml_node<> *pSignalNode = pSignalsNode->first_node("Signal");

    for (int idxSig = 0; idxSig < nNumSignals; idxSig++) {
        if (NULL != pSignalNode) {
// printf("\t\t\tparse_xml_obd_signal, idxSig = %d\n", idxSig);
            // 信号上传及保存相关
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].bIsSend = 0;
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].strOutName = "";
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].bIsSave = 0;
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].strSaveName = "";
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].strOBDSignalName = pSignalNode->first_node("Name")->value();
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].strOBDSignalUnit = pSignalNode->first_node("Unit")->value();
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].strOBDSignalFormat = "";
            // 信号值解析相关
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].nStartBit = atoi(pSignalNode->first_node("StartBit")->value());
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].nLen = atoi(pSignalNode->first_node("Length")->value());
            // 默认大端
            rapidxml::xml_node<> *pOrder = pSignalNode->first_node("ByteOrder");
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].nByteOrder = (OBD_ByteOrder)(pOrder ? atoi(pOrder->value()) : 0);
            
            char *pStrType = pSignalNode->first_node("Type")->value();
            if (strcmp("SIGNED", pStrType) == 0) {
                pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].nType = OBD_SIGNED;
            } else if (strcmp("UNSIGNED", pStrType) == 0) {
                pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].nType = OBD_UNSIGNED;
            } else if (strcmp("FLOAT", pStrType) == 0) {
                pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].nType = OBD_FLOAT;
            } else if (strcmp("DOUBLE", pStrType) == 0) {
                pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].nType = OBD_DOUBLE;
            }

            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].StcFix.dFactor = atof(pSignalNode->first_node("SignalFactor")->value());
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].StcFix.dOffset = atof(pSignalNode->first_node("SignalOffset")->value());
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].StcFix.dDenominator = atof(pSignalNode->first_node("SignalDenominator")->value());
            pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].StcFix.dFinalOffest = atof(pSignalNode->first_node("SignalFinalOffest")->value());
// printf("\t\t\t\tsignal name: %s\n", pOBD_Mode01Signal[pOBD_Mode01PID->nNumSignal].strOBDSignalName.c_str());
            pOBD_Mode01PID->nNumSignal++;
            
            pSignalNode = pSignalNode->next_sibling("Signal");
        }
    }

    if (0 == pOBD_Mode01PID->nNumSignal) {
        delete [] pOBD_Mode01PID->pOBD_Mode01Signal;
        return -1;
    }

    return 0;
}

int parse_xml_obd_pid(rapidxml::xml_node<> *pPackageNode, const char *ChanName) {

    rapidxml::xml_node<> *pNode = NULL;
 
    char *pStrMsgID = pPackageNode->first_node("RequestID")->value();
    uint32_t nRequestID = strtoul(pStrMsgID, NULL, 16);
    pStrMsgID = pPackageNode->first_node("ResponseID")->value();
    uint32_t nResponseID = strtoul(pStrMsgID, NULL, 16);
// printf("\tnRequestID = %x, nResponseID = %x\n", nRequestID, nResponseID);

    pNode = pPackageNode->first_node("PIDs");
    if (NULL != pNode) {
        int nPIDs = atoi(pNode->first_attribute("num")->value());
        if (nPIDs <= 0) return 0;

        rapidxml::xml_node<> *pPIDNode = pNode->first_node("PID");

        OBD_Mode01RecvPackage *pOBD_Mode01RecvPackage = new (std::nothrow)OBD_Mode01RecvPackage;
        pOBD_Mode01RecvPackage->pOBD_PID = new (std::nothrow)OBD_Mode01PID[nPIDs];
        pOBD_Mode01RecvPackage->nNumPID = 0;
        if ( (NULL == pOBD_Mode01RecvPackage) || (NULL == pOBD_Mode01RecvPackage->pOBD_PID) ) return -1;

        OBD_Mode01PID *pOBD_Mode01PID = pOBD_Mode01RecvPackage->pOBD_PID;
        for (int idxPID = 0; idxPID < nPIDs; idxPID++) {
// printf("\t\tparse_xml_obd_pid, idxPID = %d\n", idxPID);
            if (NULL == pPIDNode) {
                break;
            }

            if (0 == parse_xml_obd_signal(pPIDNode, &pOBD_Mode01PID[idxPID]) ) {
                pOBD_Mode01RecvPackage->nNumPID++;
            }

            pPIDNode = pPIDNode->next_sibling("PID");
        }

        if (pOBD_Mode01RecvPackage->nNumPID) {
            std::map<uint32_t, OBD_Mode01RecvPackage*> obd_rsp_map;
            obd_rsp_map[nResponseID] = pOBD_Mode01RecvPackage;

            // 查找该通道
            Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
            if (ItrChan != g_Channel_MAP.end()) {

                OBD_MAP_MsgID_Mode01_RMsgPkg::iterator ItrReqID = (ItrChan->second).CAN.OBDII.mapOBD.find(nRequestID);
                // 该 nRequestID 已经存在
                if (ItrReqID != (ItrChan->second).CAN.OBDII.mapOBD.end()) {
                    // TODO：是否要检查 nResponseID 是否存在
                    ItrReqID->second[nResponseID] = pOBD_Mode01RecvPackage;
// printf("nRequestID(%x) exist, nResponseID = %x.\n", nRequestID, nResponseID);
                } else {
                    (ItrChan->second).CAN.OBDII.mapOBD[nRequestID] = obd_rsp_map;
// printf("nRequestID(%x) not exist, nResponseID = %x\n", nRequestID, nResponseID);
                }

            } else {
                OBD_MAP_MsgID_Mode01_RMsgPkg MapMsgIDMode01RMsgPkg;
                MapMsgIDMode01RMsgPkg[nRequestID] = obd_rsp_map;
                g_Channel_MAP[ChanName].CAN.OBDII.mapOBD = MapMsgIDMode01RMsgPkg;
// printf("%s not exist, nRequestID = %x, nResponseID = %x\n", ChanName, nRequestID, nResponseID);
            }
        } else {
            delete [] pOBD_Mode01RecvPackage->pOBD_PID;
            delete pOBD_Mode01RecvPackage;
        }
        
    }

    return 0;
}


int parse_xml_obd_package(rapidxml::xml_node<> *pOBDNode, const char *ChanName, int nNumPacks) {
    // if (NULL == pOBDNode) return -1;
    rapidxml::xml_node<> *pNode = NULL;

    // 默认使能 singlePidRequests
    pNode = pOBDNode->first_node("singlePidRequests");
    bool enableSinglePid = true;

    if (NULL != pNode) {
        char *pEnable = pNode->value();
        if (0 == strncmp("false", pEnable, strlen("false"))) {
            enableSinglePid = false;
        }
    }
    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(ChanName);
    if (ItrChan != g_Channel_MAP.end()) {
        (ItrChan->second).CAN.OBDII.enableSinglePidRequest = enableSinglePid;
    }
// printf("enableSinglePid = %d, OBDII.enableSinglePidRequest = %d\n", enableSinglePid, (ItrChan->second).CAN.OBDII.enableSinglePidRequest);
    if (nNumPacks) {
        // parse Packages
        pNode = pOBDNode->first_node("Package");
        while (nNumPacks-- && pNode) {
// printf("nNumPacks = %d\n", nNumPacks);
            parse_xml_obd_pid(pNode, ChanName);
            pNode = pNode->next_sibling("Package");
        }
    }

#if 0
    printf("ChanName: %s\n", ChanName);

    OBD_MAP_MsgID_Mode01_RMsgPkg::iterator ItrOBD = (ItrChan->second).CAN.OBDII.mapOBD.begin();
    for (; ItrOBD != (ItrChan->second).CAN.OBDII.mapOBD.end(); ItrOBD++) {

        printf("\tREQID = %x\n", ItrOBD->first);
        std::map<uint32_t, OBD_Mode01RecvPackage*>::iterator ItrRSP = (ItrOBD->second).begin();
        for (; ItrRSP != (ItrOBD->second).end(); ItrRSP++) {

            OBD_Mode01RecvPackage* pOBD_RPkg = ItrRSP->second;
            printf("\t\tRSPID = %x, PID num = %d\n", ItrRSP->first, pOBD_RPkg->nNumPID);

            OBD_Mode01PID* pOBD_PID = pOBD_RPkg->pOBD_PID;
            for (int idxPid = 0; idxPid < pOBD_RPkg->nNumPID; idxPid++) {

                printf("\t\t\tPID = %d, size = %d, signals num = %d\n", pOBD_PID[idxPid].nPID, pOBD_PID[idxPid].nByteSize, pOBD_PID[idxPid].nNumSignal);

                OBD_Mode01Signal*  pOBD_Sig = pOBD_PID[idxPid].pOBD_Mode01Signal;
                for (int idxSig = 0; idxSig < pOBD_PID[idxPid].nNumSignal; idxSig++) {

                    printf("\t\t\t\tsignal name: %s\n", pOBD_Sig[idxSig].strOBDSignalName.c_str());
                }
            }
        }

    }
#endif

    return 0;
}