
#include <thread>
#include <cstring>

#include "DBC/DBC.h"
#include "ConvertData/ConvertData.h"
#include "DevConfig/PhyChannel.hpp"

extern Channel_MAP g_Channel_MAP;
// extern CAN_UOMAP_ChanName_RcvPkg g_CAN_UOMAP_ChanName_RcvPkg;
extern stu_CAN_UOMAP_ChanName_RcvPkg g_stu_CAN_UOMAP_ChanName_RcvPk;

DBC::DBC(const canInfo& pCANInfo) 
    : pCANInfo(pCANInfo)
{
    bReceiveThreadRunning = true;
}
        
DBC::~DBC() {
    bReceiveThreadRunning = false;
}
        
int DBC::Init() {
    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(pCANInfo.nChanName);
    // printf("%s\n", pCANInfo.nChanName);
    if (ItrChan == g_Channel_MAP.end()) return 0;

	COM_MAP_MsgID_RMsgPkg::iterator ItrDBC = (ItrChan->second).CAN.mapDBC.begin();
	for (; ItrDBC != (ItrChan->second).CAN.mapDBC.end(); ItrDBC++) {
        vecDBCID.emplace_back(ItrDBC->first);
    }
#if 0
    // debug
    printf("Channel %s All DBC CAN ID : ", pCANInfo.nChanName);
    for (uint32_t i = 0; i < vecDBCID.size(); i++) {
        printf("0x%02X  ", vecDBCID[i]);
    }
    printf("\n");
#endif
    std::thread rcv_thread{&DBC::Receive_Thread, this};
    char thread_name[16] = {0};
    strncat(thread_name, pCANInfo.nChanName, strlen(pCANInfo.nChanName));
    strncat(thread_name, "DBC receive", strlen("DBC receive"));
    pthread_setname_np(rcv_thread.native_handle(), thread_name);
    rcv_thread.detach();

    return 0;
}

void DBC::Receive_Thread() {
    while (bReceiveThreadRunning) {
        // 加锁
        std::lock_guard<std::mutex> lck(g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer_lock);
        CAN_UOMAP_ChanName_RcvPkg::iterator ItrChan = g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer.find(pCANInfo.nChanName);
        if (ItrChan == g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer.end()) {
            // printf("not found!\n");
            continue;
        }

        // 查找缓冲区是否存在 DBC ID
        CAN_UOMAP_MsgID_Buffer::iterator ItrMsgID = (ItrChan->second).begin();
        for (; ItrMsgID != (ItrChan->second).end(); ItrMsgID++) {
            // printf("(ItrChan->second).size() = %d\n", (ItrChan->second).size());
            std::vector<uint32_t>::iterator ItrVecDBCID = vecDBCID.begin();
            // printf("vecDBCID.size() = %d\n", vecDBCID.size());
            for (; ItrVecDBCID != vecDBCID.end(); ItrVecDBCID++) {
                // printf("ItrMsgID->first = %02X\n", ItrMsgID->first);
                // printf("*ItrVecDBCID = %02X\n", (*ItrVecDBCID));
                if (ItrMsgID->first == *ItrVecDBCID) break;
            }
            if (ItrVecDBCID != vecDBCID.end()) break;
        }
        // 不存在需要的 ID
        if (ItrMsgID == (ItrChan->second).end()) continue;
        
        // 缓冲区存在需要的 ID
        CANReceive_Buffer RcvTemp = ItrMsgID->second;
#if 0
        printf("DBC receive_thread ok!\n");
        printf("ID: %#X\n", RcvTemp.can_id);
        for (int i = 0; i < RcvTemp.can_dlc; i++) {
            printf("%02X ", (RcvTemp.can_data[i] & 0xFF));
        }
        printf("\n");
#endif
        ItrChan->second.erase(ItrMsgID);
        can_dbc_convert2msg(pCANInfo.nChanName, RcvTemp.can_id, RcvTemp.can_data);
    }
}
        

        
