/*************************************************************************
	> File Name: CCP.h
	> Author: 
	> Mail: 
	> Created Time: Wed 06 Jan 2021 03:45:29 PM CST
 ************************************************************************/

#ifndef _CCP_H
#define _CCP_H

#include "CCPMessage.h"
#include <functional>

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

class CROMessage {
private:
	// protocol version
	// CCP_WORD CCP_PRO_VER;
	// CCP_CRO_ID CRO_ID;
	// CCP_DTO_ID DTO_ID;
	// CCP_Byte_Order Byte_Order;
	// CCP_Slave_Addr ccp_slave_addr_;	/* Intel */
	const CCP_SlaveData ccp_salve_data_;

	CROMsgType CROType_;
	CCP_CTR CTR;
	
	CCP_DWORD CRM_Num_;
	CCP_ByteVector CRM_Message_;
	CCP_BYTE SendStatus_;
	CCP_BYTE CTR_Loop_;

	// Store the CTR of CC 
	CCP_BYTE CTR_CC[256];

	typedef std::function<int(CCP_DWORD, CCP_BYTE, CCP_ByteVector)> CCP_SendFunc;
	CCP_SendFunc CANSend;
protected:

public:

private:
	inline void SetCROType(CROMsgType type) {
		CROType_ = type;
	}
	inline CCP_CTR GetCTR() {
		return CTR;
	}

	CCP_RESULT CCPMsgSend(CCP_ByteVector CCP_Msg);
	void CRMRcv(CCP_ByteVector CRM_Msg);
	void EVENTRcv(CCP_ByteVector EVENT_Msg);
public:
	inline CCP_Slave_Addr GetSlaveAddr() {
		return ccp_salve_data_.nEcuAddress;
	}
	inline CCP_CRO_ID GetCroID() {
		return ccp_salve_data_.nIdCRO;
	}
	inline CCP_DTO_ID GetDtoID() {
		return ccp_salve_data_.nIdDTO;
	}
	inline bool GetSalveOrder() {
		return ccp_salve_data_.bIntelFormat;
	}
	inline CCP_WORD GetSalveProtocolVersion() {
		return ccp_salve_data_.nProVersion;
	}

	CROMessage(const CCP_SlaveData ccp_salve_data);
	int Init();
	void SetCCPSendFunc(CCP_SendFunc);

	void Rcv(CCP_ByteVector);
	void CCPMsgRcv(CCP_DWORD CAN_ID, CCP_ByteVector CCP_Msg);
	virtual void DAQRcv(CCP_DWORD CAN_ID, CCP_ByteVector DAQ_Msg) = 0;

	CCP_RESULT CRO_CONNECT(CCP_Slave_Addr Addr_Station);
	CCP_RESULT CRO_GET_CCP_VERSION();
	CCP_RESULT CRO_DISCONNECT(CCP_Slave_Addr Addr_Station);
	CCP_RESULT CRO_SET_S_STATUS(CCP_Session_Status session_status);
	CCP_RESULT CRO_GET_DAQ_SIZE(CCP_BYTE DAQ_NO, CCP_DTO_ID DTO_ID, CCP_BYTE *Size, CCP_BYTE *FirstPID);
	CCP_RESULT CRO_SET_DAQ_PTR(CCP_BYTE DAQ_NO, CCP_BYTE ODT_NO, CCP_BYTE Element_NO);
	CCP_RESULT CRO_WRITE_DAQ(CCP_BYTE Size_Element, CCP_BYTE Addr_Ext_Element, CCP_DWORD Addr_Element);
	CCP_RESULT CRO_START_STOP(CCP_BYTE Mode, CCP_BYTE DAQ_NO, CCP_BYTE Last_ODT_NO, CCP_BYTE Event_Channel_NO, CCP_WORD Trans_Rate_Pre);
	
};

#endif
