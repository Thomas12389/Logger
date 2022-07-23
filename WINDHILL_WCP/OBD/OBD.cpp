
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

OBD::OBD(const canInfo& pCANInfo, bool enableSinglePidRequest) 
    : pCANInfo(pCANInfo)
    , m_enableSinglePidRequest(enableSinglePidRequest)
{
    bReceiveThreadRunning = true;
    fifo_path = "/tmp/";
    nOBDStatus_ = OBDStatus::OBD_INIT;
}
        
OBD::~OBD() {
    bReceiveThreadRunning = false;
    unlink(fifo_path.c_str());
}
   
int OBD::Init() {
    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(pCANInfo.nChanName);
    // printf("%s\n", pCANInfo.nChanName);
    if (ItrChan == g_Channel_MAP.end()) return 0;

    OBD_MAP_MsgID_Mode01_RMsgPkg::iterator ItrOBD = (ItrChan->second).CAN.OBDII.mapOBD.begin();
	for (; ItrOBD != (ItrChan->second).CAN.OBDII.mapOBD.end(); ItrOBD++) {

        std::map<uint32_t, OBD_Mode01RecvPackage*>::iterator ItrRSP = (ItrOBD->second).begin();
        for (; ItrRSP != (ItrOBD->second).end(); ItrRSP++) {
            // 将 PID 存储起来
            OBD_Mode01PID *pOBD_Mode01PID = (ItrRSP->second)->pOBD_PID;
            for (uint8_t i = 0; i < (ItrRSP->second)->nNumPID; i++) {
                m_map_reqid_rspid_vpid[ItrOBD->first][ItrRSP->first].emplace_back(pOBD_Mode01PID[i].nPID);
            }            
// printf("REQID = %x, RSPID = %x, PID vector size = %d\n", ItrOBD->first, ItrRSP->first, m_map_reqid_rspid_vpid[ItrOBD->first][ItrRSP->first].size());

            // ResponseID 已存在，不重复添加
            if (m_vecOBDID.end() != std::find(m_vecOBDID.begin(), m_vecOBDID.end(), ItrRSP->first)) continue;
            m_vecOBDID.emplace_back(ItrRSP->first);
        }
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
    can_register(pCANInfo.nChanName, protocol_type::PRO_OBD, {fifo_path, m_vecOBDID});
#if 0
    // debug
    printf("Channel %s All OBD CAN ID : ", pCANInfo.nChanName);
    for (uint32_t i = 0; i < m_vecOBDID.size(); i++) {
        printf("0x%02X  ", m_vecOBDID[i]);
    }
    printf("\n");
#endif
    // 接收线程
    std::thread rcv_thread{&OBD::Receive_Thread, this};
    char recv_thread_name[16] = {0};
    strncat(recv_thread_name, pCANInfo.nChanName, strlen(pCANInfo.nChanName));
    strncat(recv_thread_name, "OBD recv", strlen("OBD recv"));
    pthread_setname_np(rcv_thread.native_handle(), recv_thread_name);
    rcv_thread.detach();
    // 发送线程
    std::thread send_thread{&OBD::Send_Thread, this};
    char send_thread_name[16] = {0};
    strncat(send_thread_name, pCANInfo.nChanName, strlen(pCANInfo.nChanName));
    strncat(send_thread_name, "OBD send", strlen("OBD send"));
    pthread_setname_np(send_thread.native_handle(), send_thread_name);
    send_thread.detach();

    return 0;
}

int OBD::getEngineCoolantTemperature() {
    uint8_t eff_len = 0x02;
    m_serviceID = 0x01;
    m_pid = 0x05;
    uint8_t OBD_getEngineCoolantTemperature[8] = {eff_len, m_serviceID, m_pid, 0x00, 0x00, 0x00, 0x00, 0x00};
    return send_obd(0x7DF, OBD_getEngineCoolantTemperature);
}

int OBD::getEngineRPM() {
    uint8_t eff_len = 0x02;
    m_serviceID = 0x01;
    m_pid = 0x0C;
    uint8_t OBD_getEngineRPM[8] = {eff_len, m_serviceID, m_pid, 0x00, 0x00, 0x00, 0x00, 0x00};
    return send_obd(0x7DF, OBD_getEngineRPM);
}

int OBD::getAbsoluteThrottlePosition() {
    uint8_t eff_len = 0x02;
    m_serviceID = 0x01;
    m_pid = 0x11;
    uint8_t OBD_getAbsoluteThrottlePosition[8] = {eff_len, m_serviceID, m_pid, 0x00, 0x00, 0x00, 0x00, 0x00};
    return send_obd(0x7DF, OBD_getAbsoluteThrottlePosition);
}

int OBD::getAmbientAirTemperture() {
    uint8_t eff_len = 0x02;
    m_serviceID = 0x01;
    m_pid = 0x46;
    uint8_t OBD_getAmbientAirTemperture[8] = {eff_len, m_serviceID, m_pid, 0x00, 0x00, 0x00, 0x00, 0x00};
    return send_obd(0x7DF, OBD_getAmbientAirTemperture);
}

int OBD::send_obd(uint32_t nSendID, uint8_t *byteArray) {
	// Time out to response(ms)
	int Ms_count = 500;
    m_request_timeout = false;
	// Send
	nOBDStatus_ = OBD_RSP_PENDING;
	if (0 != can_send(pCANInfo.nChanName, pCANInfo.nFd, nSendID, nOBDSendLength, byteArray)) {
		return -1;
	}
    std::int64_t start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    std::int64_t end = start;
	while ((nOBDStatus_ == OBD_RSP_PENDING) && (end - start <= Ms_count)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
        end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
	if (end - start > Ms_count) {
		// printf("OBD response Timeout!!!\n");
        m_request_timeout = true;
		return -1;
	}

	return 0;
}
/*
int OBD::recv_obd(uint32_t id, uint8_t len, uint8_t *byteArray) {

    m_mode1_rsp_data.fill(0x00);
    nOBDStatus_ = OBD_FREE;
    // 有效性检查
    uint8_t response_type = byteArray[1];
    if (0x7F == response_type) {
        return -1;
    }
    
    for (int i = 0; i < len; i++) {
        m_mode1_rsp_data[i] = byteArray[i];
    }
    can_obd_convert2msg(pCANInfo.nChanName, id, byteArray);

    return 0;
}
*/

/*seconds: the seconds; mseconds: the micro seconds*/
static void setTimer(int seconds, int mseconds)
{
    struct timeval temp;
 
    temp.tv_sec = seconds;
    temp.tv_usec = mseconds;
 
    select(0, NULL, NULL, NULL, &temp);
 
    return ;
}



void OBD::Send_Thread() {
    int retval = 0;
    bool send_error = false;
    m_serviceID = 0x01;

    while (OBD_Run && bReceiveThreadRunning) {

        if (send_error) {
            send_error = false;
        }
        
        setTimer(0, 10000);
        
        MAP_REQID_RSPID_VPID::iterator ItrREQ = m_map_reqid_rspid_vpid.begin();

        for (; ItrREQ != m_map_reqid_rspid_vpid.end(); ItrREQ++) {
            m_currentReqID = ItrREQ->first;
            
            std::map<uint32_t, std::vector<uint8_t> >::iterator ItrRSP = (ItrREQ->second).begin();
            for (; ItrRSP != (ItrREQ->second).end(); ItrRSP++) {
                // 设置期望的 ResponseID
                m_currentExpectedRspID = ItrRSP->first;

                if (m_enableSinglePidRequest) {
                    // 每个请求只有一个 PID
                    for (uint8_t i = 0; i < (ItrRSP->second).size(); i++) {
                        // 设置欲获取的 PID
                        m_currentPidList.clear();
                        m_currentPidList.emplace_back((ItrRSP->second)[i]);

                        uint8_t singlePidData[8] = {0x02/* 有效长度 */, m_serviceID, (ItrRSP->second)[i]/* PID */, 0x00, 0x00, 0x00, 0x00, 0x00};
                        retval = send_obd(m_currentReqID, singlePidData);
                        if (-1 == retval) {
                            send_error = true;
                            break;
                        }
                    }

                } else {
                    // ISO 15765-4 协议规定 Request 的有效数据长度最大为 7
                    // 一个请求包含多个 PID
                    int left_pid_cnt = (ItrRSP->second).size();
                    int idx_pid = 0;
                    while (left_pid_cnt > 0) {
                        // 该条请求的 PID 个数
                        uint8_t pid_cnt = (left_pid_cnt >= 6) ? 6 : left_pid_cnt;
                        // 设置欲获取的 PID
                        m_currentPidList.clear();
                        m_currentPidList.insert(m_currentPidList.end(), (ItrRSP->second).begin() + idx_pid, (ItrRSP->second).begin() + (idx_pid + pid_cnt));
                        // 计算有效长度
                        uint8_t eff_len = m_currentPidList.size() + sizeof(m_serviceID);
                        uint8_t mulPidData[8] = {eff_len, m_serviceID};
                        // 复制 PID
                        std::list<uint8_t>::iterator ItrCurList = m_currentPidList.begin();
                        for (uint8_t i = 0; i < pid_cnt; i++, ItrCurList++) {
                            mulPidData[i + 2] = *ItrCurList;
                        }
                        // std::copy(std::begin(m_currentPidList), std::end(m_currentPidList), std::begin(mulPidData) + 2);

                        retval = send_obd(m_currentReqID, mulPidData);
                        if (-1 == retval) {
                            send_error = true;
                            break;
                        }

                        // 计算剩余的 PID 个数
                        left_pid_cnt -= pid_cnt;
                        idx_pid += pid_cnt;
                    }
                }

                if (send_error) {
                    break;
                }
            }

            if (send_error) {
                break;
            }
        }
        /*
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
        */
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

        // 已经超时
        if (m_request_timeout) continue;
        // 不是当前期望的 ID
        if (buffer.can_id != m_currentExpectedRspID) continue;

#if 0        
        printf("%s read -- buffer.len = %d, data: ", pCANInfo.nChanName, buffer.can_dlc);
        for (auto i = 0; i < buffer.can_dlc; i++) {
            printf( "%02X ", buffer.can_data[i]);
        }
        printf("\n");
#endif
        uint8_t response_id = 0;                            // response_id = sercice ID + 0x40
        uint8_t offest = 0;                                 // response_id 偏移
        uint32_t frame_data_len = 0;                        // 帧数据长度
        OBDFrameType frame_type = OBDFrameType::SF_FRAME;   // 帧类型
        uint8_t idx_data = 0;                               // m_mode1_rsp_data 索引
        /* 判断帧类型 */
        if ( !(buffer.can_data[0] & 0xF0) ) {
            // 单帧
            offest = 1;
            frame_data_len = buffer.can_data[0] & 0x0F;
            // 帧长度大于 8（OBD mode1 中目前不会出现）
            if (!frame_data_len) {
                offest += 1;
                frame_data_len = buffer.can_data[1];
            }
        } else if ( 0x10 == (buffer.can_data[0] & 0xF0) ) {
            // 首帧
            frame_type = OBDFrameType::FF_FRAME;
            offest = 2;
            frame_data_len = (uint16_t)(buffer.can_data[0] & 0x0F) << 8 | buffer.can_data[1];
            // 长度大于 4095（OBD mode1 中目前不会出现）
            if (!frame_data_len) {
                offest += 4;
                frame_data_len = ( (uint32_t)(buffer.can_data[2]) << 24 ) | ( (uint32_t)(buffer.can_data[3]) << 16 ) |
                                ( (uint32_t)(buffer.can_data[4]) << 8 ) | ( (uint32_t)(buffer.can_data[5]) << 0 );
            }
        } else if ( 0x20 == (buffer.can_data[0] & 0xF0) ) {
            // 连续帧
            frame_type = OBDFrameType::CF_FRAME;
            continue;
        }

        // 若有错误， buffer.can_data[offest] 应为 0x7F
        // Mode1 中仅初始化序列会回复 0x7F 0x01 0x21
        response_id = buffer.can_data[offest];
        if ( response_id != (m_serviceID + 0x40) ) {
            continue;
        }

        uint32_t final_data_len = 0;                // 传去解析的数据长度（包含 PID）
        uint32_t has_recv_len = 0;                  // 已经接收的长度
        uint8_t first_pid_offest = offest + 1;      // 首个 PID 偏移
        
        // 清空缓存
        memset(m_mode1_rsp_data, 0, sizeof(m_mode1_rsp_data));

        // 传出解析的数据长度，帧数据长度减去 response_id 的大小即可
        final_data_len = frame_data_len - sizeof(response_id);
#if 1        
        if (frame_type == OBDFrameType::SF_FRAME) {
            // 单帧
            has_recv_len = final_data_len;
            // 复制数据
            // memcpy(m_mode1_rsp_data, &(buffer.can_data[first_pid_offest]), final_data_len);
            std::copy(std::begin(buffer.can_data) + first_pid_offest, 
                    std::begin(buffer.can_data) + first_pid_offest + final_data_len, std::begin(m_mode1_rsp_data));
        } else if (frame_type == OBDFrameType::FF_FRAME) {
            // 首帧
            uint8_t cframe_sn = 1;              // 连续帧序号
            
            has_recv_len = 8 - first_pid_offest;      // 首帧中接收的有效数据长度
            // 复制首帧数据
            std::copy(std::begin(buffer.can_data) + first_pid_offest, 
                    std::begin(buffer.can_data) + first_pid_offest + has_recv_len, std::begin(m_mode1_rsp_data) + idx_data);
            idx_data += has_recv_len;
            // 回复流控帧（FC）
            uint8_t FC_data[8] = {0x30, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
            can_send(pCANInfo.nChanName, pCANInfo.nFd, m_currentReqID, sizeof(FC_data), FC_data);
            // TODO:增加超时检测，接收完所有数据
            // 设置读为非阻塞
            int flags = fcntl(r_fd, F_GETFL);
            flags |= O_NONBLOCK;
            fcntl(r_fd, F_SETFL, flags);
            while ( (has_recv_len < final_data_len) && !m_request_timeout ) {
                nbytes = read(r_fd, &buffer, sizeof(buffer));
                if (nbytes <= 0) {
                    perror("CF read");
                    continue;
                }

                // ID 不对
                if (buffer.can_id != m_currentExpectedRspID) continue;
                // 非连续帧（CF）
                if ( (buffer.can_data[0] & 0xF0) != 0x20) continue;
                // 连续帧序号错误
                if ( (buffer.can_data[0] & 0x0F) != cframe_sn) {
                    break;
                }

                cframe_sn = (cframe_sn + 1) & 0x0F; // (cframe_sn + 1) % 16;
                // 连续帧中的有效数据，去掉 byte[0]，该字节为连续帧标志及序号
                uint8_t temp_len = (final_data_len - has_recv_len > 8) ? (buffer.can_dlc - 1) : (final_data_len - has_recv_len);
                std::copy(std::begin(buffer.can_data) + 1, std::begin(buffer.can_data) + 1 + temp_len, std::begin(m_mode1_rsp_data) + idx_data);

                has_recv_len += temp_len;
                idx_data += temp_len;
            }
            // 恢复阻塞
            flags = fcntl(r_fd, F_GETFL, 0);
            flags &= ~O_NONBLOCK;
            fcntl(r_fd, F_SETFL, flags);

            // 因超时或连续帧序号错误退出
            if ( (has_recv_len < final_data_len) || m_request_timeout) {
                // m_request_timeout = false;
                continue;
            }
        }

        nOBDStatus_ = OBD_FREE;
#if 0
        // 打印当前请求及回复
        printf("m_currentReqID = %x, m_currentExpectedRspID = %x， final_data_len = %u\n", m_currentReqID, m_currentExpectedRspID, final_data_len);
        std::list<uint8_t>::iterator temp_itr = m_currentPidList.begin();
        printf("m_currentPidList: { ");
        for (; temp_itr != m_currentPidList.end(); temp_itr++) {
            printf("%x ", *temp_itr);
        }
        printf("}\n");

        printf("m_mode1_rsp_data: { ");
        for (uint8_t i = 0; i < final_data_len; i++) {
            printf("%x ", m_mode1_rsp_data[i]);
        }
        printf("}\n");        

#endif

        can_obd_convert2msg(pCANInfo.nChanName, m_currentReqID, m_currentExpectedRspID, m_currentPidList, final_data_len, m_mode1_rsp_data);

#endif
        // nOBDStatus_ = OBD_FREE;
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

        
