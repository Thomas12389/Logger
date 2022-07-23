/*************************************************************************
	> File Name: XCP.h
	> Author: 
	> Mail: 
	> Created Time: Wed 22 Dec 2021 17:49:33 PM CST
 ************************************************************************/

#ifndef _XCP_H
#define _XCP_H

#include <functional>

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "XCPMessage.h"

class M2SMessage {
private:

	M2SType M2SType_;

	XCP_ByteVector CTO_REPLY_Message_;
	XCP_BYTE SendStatus_;

	typedef std::function<int(XCP_DWORD, XCP_BYTE, XCP_ByteVector)> XCP_SendFunc;
	XCP_SendFunc CANSend;
protected:
	const XCP_SlaveData xcp_slave_data_;
public:

private:
	inline void SetM2SType(M2SType type) {
		M2SType_ = type;
	}
	// inline CCP_CTR GetCTR() {
	// 	return CTR;
	// }

	XCP_RESULT XCPMsgSend(XCP_ByteVector XCP_Msg, XCP_BYTE Length);
	void ERRRcv(XCP_ByteVector EVENT_Msg);
public:
	// inline XCP_Slave_Addr GetSlaveAddr() {
	// 	return xcp_slave_data_.nEcuAddress;
	// }
	inline XCP_Master_ID GetMasterID() {
		return xcp_slave_data_.nMasterID;
	}
	inline XCP_Slave_ID GetSlaveID() {
		return xcp_slave_data_.nSlaveID;
	}
	inline bool GetSlaveOrder() {
		return xcp_slave_data_.bIntelFormat;
	}
	inline XCP_WORD GetSlaveProtocolVersion() {
		return xcp_slave_data_.nProVersion;
	}

	M2SMessage(const XCP_SlaveData xcp_slave_data);
	int Init();
	void SetXCPSendFunc(XCP_SendFunc);

	void Rcv(XCP_ByteVector);
	void XCPMsgRcv(XCP_DWORD CAN_ID, XCP_ByteVector CCP_Msg);
	virtual void DAQRcv(XCP_DWORD CAN_ID, XCP_ByteVector DAQ_Msg) = 0;

	XCP_RESULT CMD_CONNECT(XCP_BYTE Mode, XCP_SlaveData& XCP_Slave);
	XCP_RESULT CMD_GET_VERSION();
	XCP_RESULT CMD_DISCONNECT();
	XCP_RESULT CMD_GET_STATUS();
	XCP_RESULT CMD_GET_DAQ_RESOLUTION_INFO();
	XCP_RESULT CMD_GET_DAQ_PROCESSOR_INFO(XCP_SlaveData& XCP_Slave);
	XCP_RESULT CMD_CLEAR_DAQ_LIST(XCP_WORD DAQ_NO);
	XCP_RESULT CMD_FREE_DAQ();
	XCP_RESULT CMD_ALLOC_DAQ(XCP_WORD DAQ_CNT);
	XCP_RESULT CMD_ALLOC_ODT(XCP_WORD DAQ_NO, XCP_BYTE ODT_CNT);
	XCP_RESULT CMD_ALLOC_ODT_ENTRY(XCP_WORD DAQ_NO, XCP_BYTE ODT_NO, XCP_BYTE Element_CNT);
	XCP_RESULT CMD_SET_DAQ_LIST_MODE(XCP_BYTE Mode, XCP_WORD DAQ_NO, XCP_WORD Event_Channel_NO, XCP_BYTE Trans_Rate_Pre, XCP_BYTE DAQ_Priority);
	XCP_RESULT CMD_SET_DAQ_PTR(XCP_WORD DAQ_NO, XCP_BYTE ODT_NO, XCP_BYTE Element_NO);
	XCP_RESULT CMD_WRITE_DAQ(XCP_BYTE Size_Element, XCP_BYTE BIT_OFFEST, XCP_BYTE Addr_Ext_Element, XCP_DWORD Addr_Element);
	// XCP_RESULT CRO_START_STOP(CCP_BYTE Mode, CCP_BYTE DAQ_NO, CCP_BYTE Last_ODT_NO, CCP_BYTE Event_Channel_NO, CCP_WORD Trans_Rate_Pre);
	XCP_RESULT CMD_START_STOP_DAQ_LIST(XCPStartStopMode Mode, XCP_WORD DAQ_NO);
	XCP_RESULT CMD_START_STOP_SYNCH(XCPStartStopSynchMode Mode);
};

#endif
