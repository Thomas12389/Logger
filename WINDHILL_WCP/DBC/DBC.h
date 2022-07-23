
#ifndef __DBC_H__
#define __DBC_H__

#include "tbox/CAN.h"

class DBC {
	private:
		const canInfo pCANInfo;
        bool bReceiveThreadRunning;
        std::vector<uint32_t> vecDBCID;
        std::string fifo_path;
    private:
        void Receive_Thread();
        
    public:
        DBC(const canInfo& pCANInfo);
        ~DBC();
        int Init();
};

#endif
