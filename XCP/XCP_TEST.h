
#ifndef __XCP_TEST_H__
#define __XCP_TEST_H__

#include "XCP/XCP.h"

class XCP_TEST : public M2SMessage{
	private:
		const canInfo pCANInfo;
        bool bReceiveThreadRunning;
        std::vector<uint32_t> vecDAQID;
        std::string fifo_path;

    private:
        int CAN_Send(uint32_t id, uint8_t len, XCP_ByteVector byteArray);
		void DAQRcv(uint32_t CAN_ID, XCP_ByteVector DAQ_Msg);
        void Receive_Thread();

    public:
        
    public:
        XCP_TEST(const canInfo& pCANInfo, const XCP_SlaveData& pSlaveData);
        ~XCP_TEST();
        int Init();
        int Session_Login();

        int DAQList_Initialization();
        
};

#endif
