
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <thread>

#include "OBD/OBD.h"
#include "ConvertData/ConvertData.h"
#include "DevConfig/PhyChannel.hpp"

#include "Logger/Logger.h"

extern Channel_MAP g_Channel_MAP;
extern stu_CAN_UOMAP_ChanName_RcvPkg g_stu_CAN_UOMAP_ChanName_RcvPk;

int OBD_Run = 1;

OBD::OBD(const canInfo& pCANInfo) 
    : pCANInfo(pCANInfo)
{
    bReceiveThreadRunning = true;
    fifo_path = "/tmp/";
    nOBDStatus_ = 0;
}
        
OBD::~OBD() {
    bReceiveThreadRunning = false;
    unlink(fifo_path.c_str());
}

#ifdef SELF_OBD        
int OBD::Init() {
    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(pCANInfo.nChanName);
    // printf("%s\n", pCANInfo.nChanName);
    if (ItrChan == g_Channel_MAP.end()) return 0;

	OBD_MAP_MsgID_RMsgPkg::iterator ItrOBD = (ItrChan->second).CAN.mapOBD.begin();
	for (; ItrOBD != (ItrChan->second).CAN.mapOBD.end(); ItrOBD++) {
        vecOBDID.emplace_back(ItrOBD->first);
    }
    fifo_path.append(pCANInfo.nChanName);
    fifo_path.append("_");
    fifo_path.append(protocol_enum_to_string(protocol_type::PRO_OBD));
    // RegInfo info{fifo_path, vecOBDID};
    int ret = mkfifo(fifo_path.c_str(), 0755);
    if (ret < 0 && errno != EEXIST) {
        perror("mkfifo");
        exit(1);
    }
    can_register(pCANInfo.nChanName, protocol_type::PRO_OBD, {fifo_path, vecOBDID});
#if 0
    // debug
    printf("Channel %s All OBD CAN ID : ", pCANInfo.nChanName);
    for (uint32_t i = 0; i < vecOBDID.size(); i++) {
        printf("0x%02X  ", vecOBDID[i]);
    }
    printf("\n");
#endif
    std::thread send_thread{&OBD::Send_Thread, this};
    char send_thread_name[16] = {0};
    strncat(send_thread_name, pCANInfo.nChanName, strlen(pCANInfo.nChanName));
    strncat(send_thread_name, "OBD send", strlen("OBD send"));
    pthread_setname_np(send_thread.native_handle(), send_thread_name);
    send_thread.detach();

    std::thread rcv_thread{&OBD::Receive_Thread, this};
    char recv_thread_name[16] = {0};
    strncat(recv_thread_name, pCANInfo.nChanName, strlen(pCANInfo.nChanName));
    strncat(recv_thread_name, "OBD recv", strlen("OBD recv"));
    pthread_setname_np(rcv_thread.native_handle(), recv_thread_name);
    rcv_thread.detach();

    return 0;
}

int OBD::getEngineCoolantTemperature() {
    uint8_t eff_len = 0x02;
    nfunctionID_ = 0x01;
    nSubFunction_ = 0x05;
    uint8_t OBD_getEngineCoolantTemperature[8] = {eff_len, nfunctionID_, nSubFunction_, 0x00, 0x00, 0x00, 0x00, 0x00};
    return send_obd(OBD_getEngineCoolantTemperature);
}

int OBD::getEngineRPM() {
    uint8_t eff_len = 0x02;
    nfunctionID_ = 0x01;
    nSubFunction_ = 0x0C;
    uint8_t OBD_getEngineRPM[8] = {eff_len, nfunctionID_, nSubFunction_, 0x00, 0x00, 0x00, 0x00, 0x00};
    return send_obd(OBD_getEngineRPM);
}

int OBD::getAbsoluteThrottlePosition() {
    uint8_t eff_len = 0x02;
    nfunctionID_ = 0x01;
    nSubFunction_ = 0x11;
    uint8_t OBD_getAbsoluteThrottlePosition[8] = {eff_len, nfunctionID_, nSubFunction_, 0x00, 0x00, 0x00, 0x00, 0x00};
    return send_obd(OBD_getAbsoluteThrottlePosition);
}

int OBD::getAmbientAirTemperture() {
    uint8_t eff_len = 0x02;
    nfunctionID_ = 0x01;
    nSubFunction_ = 0x46;
    uint8_t OBD_getAmbientAirTemperture[8] = {eff_len, nfunctionID_, nSubFunction_, 0x00, 0x00, 0x00, 0x00, 0x00};
    return send_obd(OBD_getAmbientAirTemperture);
}

int OBD::send_obd(uint8_t *byteArray) {
	// Time out to response(ms)
	int Ms_count = 500;
	// Send
	nOBDStatus_ = OBD_RSP_PENDING;
	if (0 != can_send(pCANInfo.nChanName, pCANInfo.nFd, nOBDSendID, nOBDSendLength, byteArray)) {
		return -1;
	}

	while ((nOBDStatus_ == OBD_RSP_PENDING) && Ms_count) {
		usleep(1000);
		--Ms_count;
	}
	if (Ms_count <= 0) {
		// printf("OBD response Timeout!!!\n");
		return -1;
	}
	
	if ((nfunctionID_ + 0x40) != OBD_Message_[1]) {
		// printf("OBD nfunctionID_ ERROR!!\n");
		return -1;
	}

	if (nSubFunction_ != OBD_Message_[2]) {
        // printf("OBD nSubFunction_ ERROR!!\n");
        return -1;
    }
	return 0;
}

int OBD::recv_obd(uint32_t id, uint8_t len, uint8_t *byteArray) {

    OBD_Message_.fill(0x00);
    nOBDStatus_ = OBD_FREE;
    // 有效性检查
    uint8_t response_type = byteArray[1];
    if (0x7F == response_type) {
        return -1;
    }
    
    for (int i = 0; i < len; i++) {
        OBD_Message_[i] = byteArray[i];
    }
    can_obd_convert2msg(pCANInfo.nChanName, id, byteArray);

    return 0;
}

void OBD::Send_Thread() {
    int retval = 0;
    while (OBD_Run && bReceiveThreadRunning) {
        retval = getEngineCoolantTemperature();
        if (-1 == retval) {
            usleep(10000);
            continue;
        }
        retval = getEngineRPM();
        if (-1 == retval) {
            usleep(10000);
            continue;
        }
        retval = getAbsoluteThrottlePosition();
        if (-1 == retval) {
            usleep(10000);
            continue;
        }
        retval = getAmbientAirTemperture();
        if (-1 == retval) {
            usleep(10000);
            continue;
        }
        // 500 ms
        usleep(500000);
    }
}

void OBD::Receive_Thread() {
    int r_fd = open(fifo_path.c_str(), O_RDONLY);
    if (r_fd < 0) {
        XLOG_ERROR("ERROR {} with {}, {}", pCANInfo.nChanName, protocol_enum_to_string(protocol_type::PRO_OBD), strerror(errno));
    }

    ssize_t nbytes = -1;
    CANReceive_Buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    while (OBD_Run && bReceiveThreadRunning) {
        nbytes = read(r_fd, &buffer, sizeof(buffer));
        if (nbytes <= 0) {
            perror("read");
            continue;
        }
#if 0        
        printf("%s read -- buffer.len = %d, data: ", pCANInfo.nChanName, buffer.can_dlc);
        for (auto i = 0; i < buffer.can_dlc; i++) {
            printf( "%02X ", buffer.can_data[i]);
        }
        printf("\n");
#endif
        recv_obd(buffer.can_id, buffer.can_dlc, buffer.can_data);

    }

#if 0
    while (OBD_Run && bReceiveThreadRunning) {
        // 10 ms
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // 加锁
        std::lock_guard<std::mutex> lck(g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer_lock);
        CAN_UOMAP_ChanName_RcvPkg::iterator ItrChan = g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer.find(pCANInfo.nChanName);
        if (ItrChan == g_stu_CAN_UOMAP_ChanName_RcvPk.can_buffer.end()) {
            // printf("not found!\n");
            continue;
        }

        // 查找缓冲区是否存在 OBD ID
        CAN_UOMAP_MsgID_Buffer::iterator ItrMsgID = (ItrChan->second).begin();
        for (; ItrMsgID != (ItrChan->second).end(); ItrMsgID++) {
            // printf("(ItrChan->second).size() = %d\n", (ItrChan->second).size());
            std::vector<uint32_t>::iterator ItrVecOBDID = vecOBDID.begin();
            // printf("vecOBDID.size() = %d\n", vecOBDID.size());
            for (; ItrVecOBDID != vecOBDID.end(); ItrVecOBDID++) {
                // printf("ItrMsgID->first = %02X\n", ItrMsgID->first);
                // printf("*ItrVecOBDID = %02X\n", (*ItrVecOBDID));
                if (ItrMsgID->first == *ItrVecOBDID) break;
            }
            if (ItrVecOBDID != vecOBDID.end()) break;
        }
        // 不存在需要的 ID
        if (ItrMsgID == (ItrChan->second).end()) continue;
        
        // 缓冲区存在需要的 ID
        CANReceive_Buffer RcvTemp = ItrMsgID->second;

    #if 0
        printf("OBD receive_thread ok!\n");
        printf("ID: %#X\n", RcvTemp.can_id);
        for (int i = 0; i < RcvTemp.can_dlc; i++) {
            printf("%02X ", (RcvTemp.can_data[i] & 0xFF));
        }
        printf("\n");
    #endif
        ItrChan->second.erase(ItrMsgID);
        recv_obd(RcvTemp.can_id, RcvTemp.can_dlc, RcvTemp.can_data);
    }
#endif

}

#endif
        

        
