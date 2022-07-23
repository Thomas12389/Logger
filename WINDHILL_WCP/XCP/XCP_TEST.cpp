#include <thread>
// #include <functional>
#include <sstream>
#include <iomanip>

#include "tbox/Common.h"

#include "ConvertData/ConvertData.h"
#include "DevConfig/PhyChannel.hpp"

#include "tbox/CAN.h"
#include "XCP/XCP_TEST.h"
#include "Logger/Logger.h"

extern Channel_MAP g_Channel_MAP;
extern stu_CAN_UOMAP_ChanName_RcvPkg g_stu_CAN_UOMAP_ChanName_RcvPk;

int XCP_Run = 1;

void XCP_TEST::DAQRcv(uint32_t CAN_ID, XCP_ByteVector DAQ_Msg) {
    uint8_t pMsg[64] = {0};
    std::copy(DAQ_Msg.begin(), DAQ_Msg.end(), pMsg);
    can_xcp_convert2msg(pCANInfo.nChanName, CAN_ID, pMsg);
}

XCP_TEST::XCP_TEST(const canInfo& pCANInfo, const XCP_SlaveData& pSlaveData)
	: M2SMessage(pSlaveData)
    , pCANInfo(pCANInfo)
{
    bReceiveThreadRunning = true;
    fifo_path = "/tmp/";
	SetXCPSendFunc(std::bind(&XCP_TEST::CAN_Send, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

XCP_TEST::~XCP_TEST() {
    bReceiveThreadRunning = false;
    unlink(fifo_path.c_str());
}

// 将所有的 DAQList ID 取出
int XCP_TEST::Init() {

    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(pCANInfo.nChanName);
    // printf("%s\n", pCANInfo.nChanName);
    if (ItrChan == g_Channel_MAP.end()) return 0;

    XCP_MAP_Slave_DAQList::iterator ItrXCP = (ItrChan->second).CAN.mapXCP.find(xcp_slave_data_);
    if (ItrXCP != (ItrChan->second).CAN.mapXCP.end()) {
        // Slave ID 也需要注册
        vecDAQID.emplace_back(GetSlaveID());
		std::vector<XCP_DAQList>::iterator ItrVecDAQList = (ItrXCP->second).begin();
		for (; ItrVecDAQList != (ItrXCP->second).end(); ItrVecDAQList++) {
            if (vecDAQID.end() != std::find(vecDAQID.begin(), vecDAQID.end(), (*ItrVecDAQList).nCANID)) continue;
            vecDAQID.emplace_back((*ItrVecDAQList).nCANID);
        }
    }

    fifo_path.append(pCANInfo.nChanName);
    fifo_path.append("_");
    fifo_path.append(protocol_enum_to_string(protocol_type::PRO_XCP));
    // 多个从机时，使用 Slave ID 区分
    fifo_path.append("_");
    std::ostringstream oss;
    oss << std::setiosflags(std::ios::uppercase) << std::hex << xcp_slave_data_.nSlaveID;
    fifo_path.append(oss.str());

    int ret = mkfifo(fifo_path.c_str(), 0755);
    if (ret < 0 && errno != EEXIST) {
        perror("mkfifo");
        XLOG_ERROR("ERROR {} with {}, {}", pCANInfo.nChanName, protocol_enum_to_string(protocol_type::PRO_XCP), strerror(errno));
        exit(1);
    }
    can_register(pCANInfo.nChanName, protocol_type::PRO_XCP, {fifo_path, vecDAQID});

#if 0
    printf("All DAQList CAN ID : ");
    for (uint32_t i = 0; i < vecDAQID.size(); i++) {
        printf("0x%02X  ", vecDAQID[i]);
    }
    printf("\n");
#endif
    // 启动接收线程
    std::thread rcv_thread_id{&XCP_TEST::Receive_Thread, this};
    char thread_name[16] = {0};
    strncat(thread_name, pCANInfo.nChanName, strlen(pCANInfo.nChanName));
    strncat(thread_name, "XCP receive", strlen("XCP receive"));
    pthread_setname_np(rcv_thread_id.native_handle(), thread_name);
    rcv_thread_id.detach();

    DAQList_Initialization();

    return 0;
}

void XCP_TEST::Receive_Thread() {
    int r_fd = open(fifo_path.c_str(), O_RDONLY);
    if (r_fd < 0) {
        XLOG_ERROR("ERROR {} with {}, {}", pCANInfo.nChanName, protocol_enum_to_string(protocol_type::PRO_XCP), strerror(errno));
    }

    ssize_t nbytes = -1;
    CANReceive_Buffer buffer;
    memset(&buffer, 0, sizeof(buffer));

    uint64_t start = getTimestamp(TIME_STAMP::NS_STAMP);
    uint64_t end = start;
    // 设置读为非阻塞
    int flags = fcntl(r_fd, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(r_fd, F_SETFL, flags);
    while (XCP_Run && bReceiveThreadRunning) {
        // 10 ms
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        end = getTimestamp(TIME_STAMP::NS_STAMP);
        if ((end - start) >= 5000000000) {
            // DAQList_Initialization();
            std::thread daq_init_thread{&XCP_TEST::DAQList_Initialization, this};
            char thread_name[16] = {0};
            strncat(thread_name, pCANInfo.nChanName, strlen(pCANInfo.nChanName));
            strncat(thread_name, "XCP DAQInit", strlen("XCP DAQInit"));
            pthread_setname_np(daq_init_thread.native_handle(), thread_name);
            daq_init_thread.detach();

            start = getTimestamp(TIME_STAMP::NS_STAMP);
        }

        nbytes = read(r_fd, &buffer, sizeof(buffer));
        if (nbytes <= 0) {
            // perror("read");
            continue;
        }
#if 0        
        printf("%s read -- buffer.len = %d, id = %" PRIu16 ", data: ", pCANInfo.nChanName, buffer.can_dlc, buffer.can_id);
        for (auto i = 0; i < buffer.can_dlc; i++) {
            printf( "%" PRIX8 " ", buffer.can_data[i]);
        }
        printf("\n");
#endif
        std::array<XCP_BYTE, 64> Msg;
        for (size_t i = 0; i < Msg.size(); i++) {
            Msg[i] = buffer.can_data[i];
        }
        XCPMsgRcv(buffer.can_id, Msg);

        start = getTimestamp(TIME_STAMP::NS_STAMP);
    }
    close(r_fd);

#if 0
    while (XCP_Run && bReceiveThreadRunning) {
        // 10 ms
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        end = getTimestamp(TIME_STAMP::NS_STAMP);
        if ((end - start) >= 5000000000) {
            DAQList_Initialization();
            start = getTimestamp(TIME_STAMP::NS_STAMP);
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

        start = getTimestamp(TIME_STAMP::NS_STAMP);
    }
#endif

}

int XCP_TEST::CAN_Send(uint32_t id, uint8_t len, XCP_ByteVector byteArray) {
	uint8_t Msg[64];
	std::copy(byteArray.begin(), byteArray.end(), Msg);
	return can_send(pCANInfo.nChanName, pCANInfo.nFd, id, len, Msg);
}

int XCP_TEST::DAQList_Initialization() {
    int xResult = 0;

    Channel_MAP::iterator ItrChan = g_Channel_MAP.find(pCANInfo.nChanName);
    if (ItrChan == g_Channel_MAP.end()) {
        // printf("not found!\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    uint32_t nMsgId = GetSlaveID();
    XCP_MAP_Slave_DAQList::iterator ItrXCP = (ItrChan->second).CAN.mapXCP.begin();
	for (; ItrXCP != (ItrChan->second).CAN.mapXCP.end(); ItrXCP++) {
		if ((ItrXCP->first).nSlaveID != nMsgId) continue;
		break;
	}
    if (ItrXCP == (ItrChan->second).CAN.mapXCP.end()) {
        // printf("ItrXCP not found!\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    XCP_SlaveData xcp_slave = ItrXCP->first;
    // 1.连接
    XCP_BYTE mode = 0x00; // 0x00: Normal, 0x01: user-defined
    xResult = CMD_CONNECT(mode, xcp_slave);
    if (-1 == xResult) {
        // fprintf(stderr, "CMD_CONNECT Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    xResult = CMD_GET_DAQ_PROCESSOR_INFO(xcp_slave);
    if (-1 == xResult) {
        // fprintf(stderr, "CMD_GET_DAQ_PROCESSOR_INFO Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }
    xResult = CMD_GET_DAQ_RESOLUTION_INFO();
    if (-1 == xResult) {
        // fprintf(stderr, "CMD_GET_DAQ_RESOLUTION_INFO Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    // 2.准备 DAQLis
    std::vector<XCP_DAQList>::iterator ItrVecDAQ = (ItrXCP->second).begin();
    // static
    for (; ItrVecDAQ != (ItrXCP->second).end(); ItrVecDAQ++) {
        xResult = CMD_CLEAR_DAQ_LIST( (*ItrVecDAQ).nDaqNO );
        if (-1 == xResult) {
            // fprintf(stderr, "CMD_ALLOC_ODT Error\n");
            XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
            return -1;
        }
    }

    // dynamic
    /*
    xResult = CMD_FREE_DAQ();
    if (-1 == xResult) {
        // fprintf(stderr, "CMD_FREE_DAQ Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    xResult = CMD_ALLOC_DAQ((ItrXCP->second).size());
    if (-1 == xResult) {
        // fprintf(stderr, "CMD_ALLOC_DAQ Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
    }

    for (; ItrVecDAQ != (ItrXCP->second).end(); ItrVecDAQ++) {
        xResult = CMD_ALLOC_ODT( (*ItrVecDAQ).nDaqNO, (*ItrVecDAQ).nOdtNum);
        if (-1 == xResult) {
            // fprintf(stderr, "CMD_ALLOC_ODT Error\n");
            XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
            return -1;
        }

        XCP_ODT *pODT = (*ItrVecDAQ).pODT;
        for (int i = 0; i < (*ItrVecDAQ).nOdtNum; i++) {
            xResult = CMD_ALLOC_ODT_ENTRY( (*ItrVecDAQ).nDaqNO, pODT[i].nOdtNO, pODT[i].nNumElements);
            if (-1 == xResult) {
                // fprintf(stderr, "CMD_ALLOC_ODT_ENTRY Error\n");
                XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
                return -1;
            }
        }
    }
    */

    // 3.配置 DAQList
    ItrVecDAQ = (ItrXCP->second).begin();
    for (; ItrVecDAQ != (ItrXCP->second).end(); ItrVecDAQ++) {
        
        XCP_ODT *pODT = (*ItrVecDAQ).pODT;
        for (uint8_t i = 0; i < (*ItrVecDAQ).nOdtNum; i++) {

            XCP_ElementStruct *pEle = pODT[i].pElement;
            for (uint8_t j = 0; j < pODT[i].nNumElements; j++) {
                xResult = CMD_SET_DAQ_PTR( (*ItrVecDAQ).nDaqNO, pODT[i].nOdtNO, j);
                if (-1 == xResult) {
                    // fprintf(stderr, "CMD_SET_DAQ_PTR Error\n");
                    XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
                    return -1;
                }

                uint8_t bit_offest = pEle[j].StcFix.cMask;
                xResult = CMD_WRITE_DAQ(pEle[j].nLen, 0xFF, pEle[j].nAddress_Ex, pEle[j].nAddress);
                if (-1 == xResult) {
                    // fprintf(stderr, "CMD_WRITE_DAQ Error\n");
                    XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
                    return -1;
                }
            }
        }
    }

    // 4.启动传输
    ItrVecDAQ = (ItrXCP->second).begin();
    for (; ItrVecDAQ != (ItrXCP->second).end(); ItrVecDAQ++) {

        xResult = CMD_SET_DAQ_LIST_MODE(0x00, (*ItrVecDAQ).nDaqNO, (*ItrVecDAQ).nEventChNO, (*ItrVecDAQ).nPrescaler, (*ItrVecDAQ).nPriority);
        if (-1 == xResult) {
            fprintf(stderr, "SET_DAQ_LIST_MODE <%d> DAQList Error\n", (*ItrVecDAQ).nDaqNO);
            XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
            return -1;
            // continue;
        }

        xResult = CMD_START_STOP_DAQ_LIST(XCPStartStopMode::SELECT, (*ItrVecDAQ).nDaqNO);
        if (-1 == xResult) {
            fprintf(stderr, "SELECT <%d> DAQList Error\n", (*ItrVecDAQ).nDaqNO);
            XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
            return -1;
            // continue;
        }
    }

    xResult = CMD_START_STOP_SYNCH(XCPStartStopSynchMode::PREPARE_START_SELECTED);
    if (-1 == xResult) {
        fprintf(stderr, "START_STOP_SYNCH DAQList <PREPARE_START_SELECTED> Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
        // continue;
    }
    xResult = CMD_START_STOP_SYNCH(XCPStartStopSynchMode::START_SELECTED);
    if (-1 == xResult) {
        fprintf(stderr, "START_STOP_SYNCH DAQList <START_SELECTED> Error\n");
        XLOG_ERROR("{} DAQList_Initialization ERROR.", pCANInfo.nChanName);
        return -1;
        // continue;
    }

    XLOG_INFO("{} DAQList_Initialization successfully.", pCANInfo.nChanName);
    return 0;
}