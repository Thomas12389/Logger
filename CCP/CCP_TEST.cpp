#include <thread>
#include <semaphore.h>
#include <functional>

#include "tbox/Common.h"

#include "ConvertData/ConvertData.h"
#include "DevConfig/PhyChannel.hpp"

#include "tbox/CAN.h"
#include "CCP/CCP_TEST.h"
#include "Logger/Logger.h"

extern Channel_MAP g_Channel_MAP;
extern stu_CAN_UOMAP_ChanName_RcvPkg g_stu_CAN_UOMAP_ChanName_RcvPk;

int CCP_Run = 1;

void CCP_TEST::DAQRcv(uint32_t CAN_ID, CCP_ByteVector DAQ_Msg) {
    uint8_t pMsg[8] = {0};
    std::copy(DAQ_Msg.begin(), DAQ_Msg.end(), pMsg);
    can_ccp_convert2msg(pCANInfo.nChanName, CAN_ID, pMsg);
}

CCP_TEST::CCP_TEST(const canInfo& pCANInfo, const CCP_SlaveData& pSalveData)
	: CROMessage(pSalveData)
    , pCANInfo(pCANInfo)
{
    bReceiveThreadRunning = true;
    bReinitializeThreadRunning = true;
	SetCCPSendFunc(std::bind(&CCP_TEST::CAN_Send, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

CCP_TEST::~CCP_TEST() {
    bReceiveThreadRunning = false;
    bReinitializeThreadRunning = false;
}

// 将所有的 DAQList ID 取出
int CCP_TEST::Init() {

    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(pCANInfo.nChanName);
    // printf("%s\n", pCANInfo.nChanName);
    if (ItrChan == g_Channel_MAP.end()) return 0;

	CCP_MAP_Salve_DAQList::iterator ItrCCP = (ItrChan->second).CAN.mapCCP.begin();
	for (; ItrCCP != (ItrChan->second).CAN.mapCCP.end(); ItrCCP++) {
			
		std::vector<CCP_DAQList>::iterator ItrVecDAQList = (ItrCCP->second).begin();
		for (; ItrVecDAQList != (ItrCCP->second).end(); ItrVecDAQList++) {
            vecDAQID.emplace_back((*ItrVecDAQList).nCANID);
        }
    }

#if 0
    printf("All DAQList CAN ID : ");
    for (uint32_t i = 0; i < vecDAQID.size(); i++) {
        printf("0x%02X  ", vecDAQID[i]);
    }
    printf("\n");
#endif
    // 启动接收线程
    std::thread rcv_thread_id{&CCP_TEST::Receive_Thread, this};
    pthread_setname_np(rcv_thread_id.native_handle(), "CCP receive");
    rcv_thread_id.detach();

    DAQList_Initialization();

    return 0;
}

void CCP_TEST::Receive_Thread() {
    uint64_t start = getTimestamp(3);
    uint64_t end = getTimestamp(3);
    while (CCP_Run && bReceiveThreadRunning) {
        end = getTimestamp(3);
        if ((end - start) >= 5000000000) {
            DAQList_Initialization();
            start = getTimestamp(3);
        }
        std::lock_guard<std::mutex> lck(g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer_lock);
        CAN_UOMAP_ChanName_RcvPkg::iterator ItrChan = g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer.find(pCANInfo.nChanName);
        if (ItrChan == g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer.end()) {
            // printf("not found!\n");
            continue;
        }

        // 查找缓冲区是否存在 DTO_ID
        CAN_UOMAP_MsgID_Buffer::iterator ItrMsgID = (ItrChan->second).find(GetDtoID());
        if (ItrMsgID == (ItrChan->second).end()) {
            // 没有需要的 DTO_ID, 检查是否有 DAQList ID
            ItrMsgID = (ItrChan->second).begin();
            for (; ItrMsgID != (ItrChan->second).end(); ItrMsgID++) {
                // printf("(ItrChan->second).size() = %d\n", (ItrChan->second).size());
                std::vector<uint32_t>::iterator ItrVecDAQID = vecDAQID.begin();
                // printf("vecDAQID.size() = %d\n", vecDAQID.size());
                for (; ItrVecDAQID != vecDAQID.end(); ItrVecDAQID++) {
                    // printf("ItrMsgID->first = %02X\n", ItrMsgID->first);
                    // printf("*ItrVecDAQID = %02X\n", (*ItrVecDAQID));
                    if (ItrMsgID->first == *ItrVecDAQID) break;
                }
                if (ItrVecDAQID != vecDAQID.end()) break;
            }
            if (ItrMsgID == (ItrChan->second).end()) continue;
        }
        
        // 缓冲区存在需要的 ID
        start = getTimestamp(3);
        CANReceive_Buffer RcvTemp = ItrMsgID->second;
        std::array<CCP_BYTE, 8> Msg;
        for (size_t i = 0; i < Msg.size(); i++) {
            Msg[i] = RcvTemp.can_data[i];
        }
#if 0
        printf("CCP receive_thread ok!\n");
        printf("ID: %#X\n", RcvTemp.can_id);
        for (int i = 0; i < Msg.size(); i++) {
            printf("%02X ", (Msg[i] & 0xFF));
        }
        printf("\n");
#endif
        // 清除已经处理过的消息，否则会影响下一条 DTO
        ItrChan->second.erase(ItrMsgID);
        CCPMsgRcv(RcvTemp.can_id, Msg);
    }
}

int CCP_TEST::CAN_Send(uint32_t id, uint8_t len, CCP_ByteVector byteArray) {
	uint8_t Msg[8];
	std::copy(byteArray.begin(), byteArray.end(), Msg);
	return can_send(pCANInfo.nFd, id, len, Msg);
}

int CCP_TEST::DAQList_Initialization() {
    int cResult = 0;
    CCP_Slave_Addr addr = GetSlaveAddr();
    cResult = CRO_CONNECT(addr);
    if (-1 == cResult) {
        // fprintf(stderr, "CRO_CONNECT Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    cResult = CRO_SET_S_STATUS(~SS_DAQ);
    if (-1 == cResult) {
        // fprintf(stderr, "CRO_SET_S_STATUS Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }
    
	Channel_MAP::iterator ItrChan = g_Channel_MAP.find(pCANInfo.nChanName);
    if (ItrChan == g_Channel_MAP.end()) {
        // printf("not found!\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    uint32_t nMsgId = GetDtoID(); // TODO
    CCP_MAP_Salve_DAQList::iterator ItrCCP = (ItrChan->second).CAN.mapCCP.begin();
	for (; ItrCCP != (ItrChan->second).CAN.mapCCP.end(); ItrCCP++) {
		if ((ItrCCP->first).nIdDTO != nMsgId) continue;
		break;
	}
    if (ItrCCP == (ItrChan->second).CAN.mapCCP.end()) {
        // printf("ItrCCP not found!\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    std::vector<CCP_DAQList>::iterator ItrVecDAQ = (ItrCCP->second).begin();
    for (; ItrVecDAQ != (ItrCCP->second).end(); ItrVecDAQ++) {
        CCP_BYTE DAQList_size = 0;
        CCP_BYTE First_PID = 0;
        cResult = CRO_GET_DAQ_SIZE((*ItrVecDAQ).nDaqNO, (*ItrVecDAQ).nCANID, &DAQList_size, &First_PID);
        if (-1 == cResult) {
            fprintf(stderr, "GET DAQList %d size Error\n", (*ItrVecDAQ).nDaqNO);
            XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
            return -1;
            // continue;
        }
        if ((*ItrVecDAQ).nOdtNum > DAQList_size ) {
            (*ItrVecDAQ).nOdtNum = DAQList_size;
            fprintf(stderr, "configure ODT numbers is too larger in <%d>th DAQList\n", (*ItrVecDAQ).nDaqNO);
        }
        (*ItrVecDAQ).nFirstPID = First_PID;

        CCP_ODT *pODT = (*ItrVecDAQ).pODT;
        for (int i = 0; i < (*ItrVecDAQ).nOdtNum; i++) {

            CCP_ElementStruct *pEle = pODT[i].pElement;
            for (int j = 0; j < pODT[i].nNumElements; j++) {
                cResult = CRO_SET_DAQ_PTR((*ItrVecDAQ).nDaqNO, i, j);
                if (-1 == cResult) {
                    fprintf(stderr, "SET_DAQ_PTR <%d>th DAQList <%d>th ODT <%d>th Element Error\n", (*ItrVecDAQ).nDaqNO, i, j);
                    XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
                    return -1;
                    // continue;
                }
                cResult = CRO_WRITE_DAQ(pEle[j].nLen, pEle[j].nAddress_Ex, pEle[j].nAddress);
                if (-1 == cResult) {
                    fprintf(stderr, "WRITE DAQ <%d>th DAQList <%d>th ODT <%d>th Element Error\n", (*ItrVecDAQ).nDaqNO, i, j);
                    XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
                    return -1;
                    // continue;
                }
            }
        }
    }

    cResult = CRO_SET_S_STATUS(SS_DAQ);
    if (-1 == cResult) {
        fprintf(stderr, "CRO_SET_S_STATUS Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    ItrVecDAQ = (ItrCCP->second).begin();
    for (; ItrVecDAQ != (ItrCCP->second).end(); ItrVecDAQ++) {
        cResult = CRO_START_STOP(MODE_START, (*ItrVecDAQ).nDaqNO, (*ItrVecDAQ).nOdtNum - 1,
                                (*ItrVecDAQ).nEventChNO, (*ItrVecDAQ).nPrescaler);
        if (-1 == cResult) {
            fprintf(stderr, "START <%d> DAQList Error\n", (*ItrVecDAQ).nDaqNO);
            XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
            return -1;
            // continue;
        }
    }
    XLOG_INFO("{} DAQList_Initialization successfully.", pCANInfo.nChanName);
    return 0;
}