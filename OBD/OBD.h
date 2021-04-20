
#ifndef __OBD_H__
#define __OBD_H__

#include "tbox/CAN.h"

enum OBDStatus : uint8_t {
	OBD_FREE = 0x01,
	OBD_RSP_PENDING = 0x02
};

class OBD {
	private:
		const canInfo pCANInfo;
        bool bReceiveThreadRunning;
        std::vector<uint32_t> vecOBDID;

        uint8_t nfunctionID_;
        uint8_t nSubFunction_;
        uint8_t nOBDStatus_;
        std::array<uint8_t, 8> OBD_Message_;

        const int nOBDSendID = 0x7DF;
        const int nOBDSendLength = 0x08;
    private:
        void Receive_Thread();
        void Send_Thread();
        int send_obd(uint8_t *byteArray);
        int recv_obd(uint32_t id, uint8_t len, uint8_t *byteArray);

        int getEngineCoolantTemperature();
        int getEngineRPM();
        int getAbsoluteThrottlePosition();
        int getAmbientAirTemperture();
        
    public:
        OBD(const canInfo& pCANInfo);
        ~OBD();
        int Init();
        int stop();
};

#endif
