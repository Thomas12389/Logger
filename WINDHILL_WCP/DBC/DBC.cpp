
#include <thread>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include "DBC/DBC.h"
#include "ConvertData/ConvertData.h"
#include "DevConfig/PhyChannel.hpp"

#include "Logger/Logger.h"

extern Channel_MAP g_Channel_MAP;
extern stu_CAN_UOMAP_ChanName_RcvPkg g_stu_CAN_UOMAP_ChanName_RcvPk;

DBC::DBC(const canInfo& pCANInfo) 
    : pCANInfo(pCANInfo)
{
    bReceiveThreadRunning = true;
    fifo_path = "/tmp/";
}
        
DBC::~DBC() {
    bReceiveThreadRunning = false;
    unlink(fifo_path.c_str());
}
        
int DBC::Init() {
    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(pCANInfo.nChanName);
    // printf("%s\n", pCANInfo.nChanName);
    if (ItrChan == g_Channel_MAP.end()) return 0;

	COM_MAP_MsgID_RMsgPkg::iterator ItrDBC = (ItrChan->second).CAN.mapDBC.begin();
	for (; ItrDBC != (ItrChan->second).CAN.mapDBC.end(); ItrDBC++) {
        // 若已经存在该 ID，则不再加入
        if (vecDBCID.end() != std::find(vecDBCID.begin(), vecDBCID.end(), ItrDBC->first)) continue;
        vecDBCID.emplace_back(ItrDBC->first);
    }
    fifo_path.append(pCANInfo.nChanName);
    fifo_path.append("_");
    fifo_path.append(protocol_enum_to_string(protocol_type::PRO_DBC));
    // RegInfo info{fifo_path, vecDBCID};
    int ret = mkfifo(fifo_path.c_str(), 0755);
    if (ret < 0 && errno != EEXIST) {
        perror("mkfifo");
        exit(1);
    }
    can_register(pCANInfo.nChanName, protocol_type::PRO_DBC, {fifo_path, vecDBCID});
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
    int r_fd = open(fifo_path.c_str(), O_RDONLY);
    if (r_fd < 0) {
        XLOG_ERROR("ERROR {} with {}, {}", pCANInfo.nChanName, protocol_enum_to_string(protocol_type::PRO_DBC), strerror(errno));
    }
    uint32_t msg_count = 0;
    ssize_t nbytes = -1;
    CANReceive_Buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    while (bReceiveThreadRunning) {
        nbytes = read(r_fd, &buffer, sizeof(buffer));
        if (nbytes <= 0) {
            perror("read");
            continue;
        }
#if 0        
        printf("%s read -- buffer.len = %d, id = %" PRIu16 ", data: ", pCANInfo.nChanName, buffer.can_dlc, buffer.can_id);
        for (auto i = 0; i < buffer.can_dlc; i++) {
            printf( "%" PRIX8 " ", buffer.can_data[i]);
        }
        printf("\n");
#endif

#if 1
        can_dbc_convert2msg(pCANInfo.nChanName, buffer.can_id, buffer.can_data);
#else
        msg_count++;
        if (msg_count % 100000 == 0) 
            printf("%s -- msg_count = %" PRIu32 "\n", pCANInfo.nChanName, msg_count);
#endif

    }

#if 0
    while (bReceiveThreadRunning) {
        // 10 ms
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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
        printf("ItrChan->first = %s\n", ItrChan->first.c_str());
        printf("ID: %#X\n", RcvTemp.can_id);
        for (int i = 0; i < RcvTemp.can_dlc; i++) {
            printf("%02X ", (RcvTemp.can_data[i] & 0xFF));
        }
        printf("\n");
#endif
        ItrChan->second.erase(ItrMsgID);
        can_dbc_convert2msg(pCANInfo.nChanName, RcvTemp.can_id, RcvTemp.can_data);
    }
#endif
}
        

        
