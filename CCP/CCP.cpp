/*************************************************************************
	> File Name: CCP.cpp
	> Author: 
	> Mail: 
	> Created Time: Wed 06 Jan 2021 03:45:34 PM CST
 ************************************************************************/
#include "CCP.h"
#include <unistd.h>
#include <cstring>

CROMessage::CROMessage(const CCP_SlaveData ccp_salve_data) 
	:ccp_salve_data_(ccp_salve_data)
{
	CTR = 0;
	CRM_Num_ = 0;
	CTR_Loop_ = 0;
	SendStatus_ = 0;
}

void CROMessage::SetCCPSendFunc(CCP_SendFunc SendF) {
	CANSend = SendF;
}

CCP_RESULT CROMessage::CCPMsgSend(CCP_ByteVector CCP_Msg) {
	CCP_BYTE CC = CCP_Msg[0];
	// Time out to ack(ms)
	int Ms_count = 0;
	int Retry_count = 0;
	switch (CC) {
		case CC_ACTION_SERVICE:
			Ms_count = 5000; break;
		case CC_BUILD_CHKSUM:
		case CC_CLEAR_MEMORY:
		case CC_MOVE:
			Ms_count = 3000; break;
		case CC_DIAG_SERVICE:
			Ms_count = 500; break;
		case CC_PROGRAM:
		case CC_PROGRAM_6:
			Ms_count = 100; break;
		default:
			Ms_count = 250; break;
	}
	// Send
	// int count = 100;
	// while ((SendStatus_ & CCP_CRM_PENDING) && count) {
	// 	usleep(1000);
	// 	--count;
	// }
	// if (count <= 0) {
	// 	return CCP_ByteVector{};
	// }
	SendStatus_ = CCP_CRM_PENDING;
	if (0 != CANSend(GetCroID(), kCCPPaylodLength, CCP_Msg)) {
		// printf("callback error!\n");
		return -1;
	}

	CTR_CC[CTR] = CC;
	while ((SendStatus_ == CCP_CRM_PENDING) && Ms_count) {
		// printf("CCP waiting ACK!!!\n");
		usleep(1000);
		--Ms_count;
	}
	if (Ms_count <= 0) {
		// printf("CCP ACK Timeout!!!\n");
		return -1;
	}
	
	// TODO: ? compare CTR vs CRM_Num_
	if (CTR != CRM_Message_[2]) {
		// printf("CCP CTR ERROR!!\n");
		return -1;
	}

	if (CTR < 0xFF) {
		CTR++;
	} else {
		CTR = 0;
		CTR_Loop_++;
	}
	// printf("CCP send OK!!\n");
	return 0;
}

void CROMessage::Rcv(CCP_ByteVector Msg) {
	// CANRcv();
	// CCPMsgRcv(id, data);
}

void CROMessage::CCPMsgRcv(CCP_DWORD CAN_ID, CCP_ByteVector CCP_Msg) {
    
	CCP_BYTE PID = CCP_Msg[0];
	switch (PID) {
		case PID_CRM:
            // printf("CRMRcv\n");
			CRMRcv(CCP_Msg);
			break;
		case PID_EVENT:
            // printf("EVENTRcv\n");
			EVENTRcv(CCP_Msg);
			break;
		default:
            // printf("DAQRcv\n");
			DAQRcv(CAN_ID, CCP_Msg);
			break;
	}
}

void CROMessage::CRMRcv(CCP_ByteVector CRM_Msg) {

	++CRM_Num_;

	CCP_BYTE CRC = CRM_Msg[1];
	switch (CRC) {
		// TODO
		case CRC_OK:
			SendStatus_ = CCP_FREE;
			CRM_Message_ = CRM_Msg;
			break;
		// C0
		case CRC_DAQ_OVERLOAD:
		// C1	wait(ACK or timeout) -- retries 2
		case CRC_CMD_BUSY:
		case CRC_DAQ_BUSY:
		case CRC_INTERNAL_TIMEOUT:
		case CRC_KEY_REQUEST:
		case CRC_STATUS_REQUEST:
		// C2	reinitialize -- retries 1
		case CRC_COLD_START_REQUEST:
		case CRC_CAL_INIT_REQUEST:
		case CRC_DAQ_INIT_REQUEST:
		case CRC_CODE_REQUEST:
		// C3	terminate -- retries 0
		case CRC_CMD_UNKNOWN:
		case CRC_CMD_SYNTAX:
		case CRC_OUT_OF_RANGE:
		case CRC_ACCESS_DENITED:
		case CRC_OVERLOAD:
		case CRC_ACCESS_LOCKED:
		case CRC_NOT_AVAILABLE:
		default: break;
	}

	// Determine CRO type by CTR
	CCP_BYTE CTR = CRM_Msg[2];
	switch (CTR_CC[CTR]) {
		// TODO
		case CC_GET_EXCHANGE_ID: {
			// CCP_BYTE LID = CRM_Msg[3];
			// CCP_BYTE DAT = CRM_Msg[4];
			// CCP_BYTE RAM = CRM_Msg[5];
			// CCP_BYTE RPM = CRM_Msg[6];
			break;
		}
		case CC_GET_DAQ_SIZE: {
			// CCP_BYTE DAQ_List_Size = CRM_Msg[3];
			// CCP_BYTE DAQ_List_First_PID = CRM_Msg[4];
			break;
		}
		default: break;
	}
}

void CROMessage::EVENTRcv(CCP_ByteVector EVENT_Msg) {
	CCP_BYTE CRC = EVENT_Msg[1];
	switch (CRC) {
		// TODO: invoke error recovery or other services
		case CRC_OK:
			break;
		default: break;
	}
}

CCP_RESULT CROMessage::CRO_CONNECT(CCP_Slave_Addr Addr_Station) {
	CCP_BYTE CC = CC_CONNECT;
	CCP_BYTEPTR PTH = (CCP_BYTEPTR)(&Addr_Station);
	CCP_ByteVector CRO_connect = {CC, CTR, *(PTH + 1), *PTH, 0x00, 0x00, 0x00, 0x00};
	// return CRO_connect;
	if (-1 == CCPMsgSend(CRO_connect)) return -1;
	return 0;
}

CCP_RESULT CROMessage::CRO_GET_CCP_VERSION() {
	CCP_BYTE CC = CC_GET_CCP_VERSION;
	CCP_BYTE MAJR = CCP_VERSION_MAJOR;
	CCP_BYTE MINR = CCP_VERSION_MINOR;
	CCP_ByteVector CRO_get_ccp_version = {CC, CTR, MAJR, MINR, 0x00, 0x00, 0x00, 0x00};
	if (-1 == CCPMsgSend(CRO_get_ccp_version)) return 1;
	if ( (CRM_Message_[3] != MAJR) || 
		((CRM_Message_[3] == MAJR) && (CRM_Message_[4] > MINR) )) {
			return -1;
	}
	return 0;
}

CCP_RESULT CROMessage::CRO_SET_S_STATUS(CCP_Session_Status Session_Status) {
	CCP_BYTE CC = CC_SET_S_STATUS;
	CCP_ByteVector CRO_set_s_tatus = {CC, CTR, Session_Status, 0x00, 0x00, 0x00, 0x00, 0x00};
	if (-1 == CCPMsgSend(CRO_set_s_tatus)) return -1;
	return 0;
}

CCP_RESULT CROMessage::CRO_GET_DAQ_SIZE(CCP_BYTE DAQ_NO, CCP_DWORD DTO_ID, CCP_BYTE *Size, CCP_BYTE *FirstPID) {
	CCP_BYTE CC = CC_GET_DAQ_SIZE;
	CCP_BYTEPTR PTID = (CCP_BYTEPTR)(&DTO_ID);
	CCP_RESULT hResult = 0;
	if (GetSalveOrder()) {
		CCP_ByteVector CRO_get_DAQ_size = {CC, CTR, DAQ_NO, 0x00, *PTID, *(PTID + 1), *(PTID + 2), *(PTID + 3)};
		hResult = CCPMsgSend(CRO_get_DAQ_size);
	} else {
		CCP_ByteVector CRO_get_DAQ_size = {CC, CTR, DAQ_NO, 0x00, *(PTID + 3), *(PTID + 2), *(PTID + 1), *PTID};
		hResult = CCPMsgSend(CRO_get_DAQ_size);
	}
	if (-1 == hResult) return -1;
	*Size = CRM_Message_[3];
	*FirstPID = CRM_Message_[4];
	return 0;
}

CCP_RESULT CROMessage::CRO_SET_DAQ_PTR(CCP_BYTE DAQ_NO, CCP_BYTE ODT_NO, CCP_BYTE Element_NO) {
	if (Element_NO < 0 || Element_NO > 7) return -1;
	CCP_BYTE CC = CC_SET_DAQ_PTR;
	CCP_ByteVector CRO_set_DAQ_ptr = {CC, CTR, DAQ_NO, ODT_NO, Element_NO, 0x00, 0x00, 0x00};
	return CCPMsgSend(CRO_set_DAQ_ptr);
}

CCP_RESULT CROMessage::CRO_WRITE_DAQ(CCP_BYTE Size_Element, CCP_BYTE Addr_Ext_Element, CCP_DWORD Addr_Element) {
	CCP_BYTE CC = CC_WRITE_DAQ;
	CCP_BYTEPTR PADDR = (CCP_BYTEPTR)(&Addr_Element);
	if (GetSalveOrder()) {
		CCP_ByteVector CRO_write_DAQ = {CC, CTR, Size_Element, Addr_Ext_Element, *PADDR, *(PADDR + 1), *(PADDR + 2), *(PADDR + 3)};
		return CCPMsgSend(CRO_write_DAQ);
	} else {
		CCP_ByteVector CRO_write_DAQ = {CC, CTR, Size_Element, Addr_Ext_Element, *(PADDR + 3), *(PADDR + 2), *(PADDR + 1), *PADDR};
		return CCPMsgSend(CRO_write_DAQ);
	}
}

CCP_RESULT CROMessage::CRO_START_STOP(CCP_BYTE Mode, CCP_BYTE DAQ_NO, CCP_BYTE Last_ODT_NO, CCP_BYTE Event_Channel_NO, CCP_WORD Trans_Rate_Pre) {
	CCP_BYTE CC = CC_START_STOP;
	CCP_BYTEPTR PTRP = (CCP_BYTEPTR)(&Trans_Rate_Pre);
	if (GetSalveOrder()) {
		CCP_ByteVector CRO_start_stop = {CC, CTR, Mode, DAQ_NO, Last_ODT_NO, Event_Channel_NO, *PTRP, *(PTRP + 1)};
		return CCPMsgSend(CRO_start_stop);
	} else {
		CCP_ByteVector CRO_start_stop = {CC, CTR, Mode, DAQ_NO, Last_ODT_NO, Event_Channel_NO, *(PTRP + 1), *PTRP};
		return CCPMsgSend(CRO_start_stop);
	}
	
}
