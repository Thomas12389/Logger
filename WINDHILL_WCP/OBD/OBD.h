
#ifndef __OBD_H__
#define __OBD_H__

#include <atomic>
#include <list>

#include "tbox/CAN.h"
//
#include "DevConfig/OBDMessageStruct.h"
//

enum OBDStatus : uint8_t {
    OBD_INIT = 0x00,
	OBD_FREE = 0x01,
	OBD_RSP_PENDING = 0x02
};

enum OBDFrameType : uint8_t {
    SF_FRAME = 0x00,
    FF_FRAME = 0x01,
    CF_FRAME = 0x02,
    FC_FRAME = 0x03
};

class OBD {
	private:
		const canInfo pCANInfo;
        bool bReceiveThreadRunning;
        std::vector<uint32_t> m_vecOBDID;   // 需要接收的所有 ID
        std::string fifo_path;

        // 是否使能单个 PID 请求
        bool m_enableSinglePidRequest {true};

        // 请求是否超时
        std::atomic<bool> m_request_timeout {false};

        // 每一对 REQ/RSP 所包含的 PID
        typedef std::map<uint32_t, std::map<uint32_t, std::vector<uint8_t> > > MAP_REQID_RSPID_VPID;
        MAP_REQID_RSPID_VPID m_map_reqid_rspid_vpid;
        // 当前所需要的 PID
        std::list<uint8_t> m_currentPidList {};
        // 当前请求 ID
        std::atomic<uint32_t> m_currentReqID {0x00};
        // 当前期望的回复 ID
        std::atomic<uint32_t> m_currentExpectedRspID {0x00};
        // 当前的 serviceID
        std::atomic<uint8_t> m_serviceID {0x00};
        std::atomic<uint8_t> m_pid {0x00};

        std::atomic<uint8_t> nOBDStatus_;
        uint8_t m_mode1_rsp_data[41] = {0x00};
        // std::array<uint8_t, 8> OBD_Message_;

        // const int nOBDSendID = 0x7DF;
        const int nOBDSendLength = 0x08;
    private:
        void Receive_Thread();
        void Send_Thread();
        int send_obd(uint32_t nSendID, uint8_t *byteArray);
        int recv_obd(uint32_t id, uint8_t len, uint8_t *byteArray);

        int getEngineCoolantTemperature();
        int getEngineRPM();
        int getAbsoluteThrottlePosition();
        int getAmbientAirTemperture();
        
    public:
        OBD(const canInfo& pCANInfo, const bool enableSinglePidRequest = true);
        ~OBD();
        int Init();
        int stop();
};

#endif
