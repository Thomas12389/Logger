
#ifndef __CCP_TEST_H__
#define __CCP_TEST_H__

#include "CCP/CCP.h"

class CCP_TEST : public CROMessage{
	private:
		const canInfo pCANInfo;
        bool bReceiveThreadRunning;
        std::vector<uint32_t> vecDAQID;
        std::string fifo_path;

    private:
        int CAN_Send(uint32_t id, uint8_t len, CCP_ByteVector byteArray);
		void DAQRcv(uint32_t CAN_ID, CCP_ByteVector DAQ_Msg);
        void Receive_Thread();

    public:
        
    public:
        CCP_TEST(const canInfo& pCANInfo, const CCP_SlaveData& pSlaveData);
        ~CCP_TEST();
        int Init();
        int Session_Login();

        int DAQList_Initialization();
        
};

#endif
