/*************************************************************************
	> File Name: CCP.cpp
	> Author: 
	> Mail: 
	> Created Time: Wed 22 Dec 2021 17:49:33 PM CST
 ************************************************************************/
#include "XCP.h"
#include <unistd.h>
#include <cstring>

M2SMessage::M2SMessage(const XCP_SlaveData xcp_slave_data) 
	:xcp_slave_data_(xcp_slave_data)
{
	SendStatus_ = 0;
}

void M2SMessage::SetXCPSendFunc(XCP_SendFunc SendF) {
	CANSend = SendF;
}

XCP_RESULT M2SMessage::XCPMsgSend(XCP_ByteVector XCP_Msg, XCP_BYTE Length) {
	XCP_BYTE msg_type = XCP_Msg[0];
	// Time out to ack(ms)
	int Ms_count = 0;
	int Retry_count = 0;
	switch (msg_type) {
		case XCP_CONNECT:
		case XCP_DISCONNECT:
		case XCP_GET_STATUS:
		case XCP_SYNCH:
		case XCP_SET_DAQ_PTR:
		case XCP_WRITE_DAQ:
		case XCP_SET_DAQ_LIST_MODE:
		case XCP_START_STOP_DAQ_LIST:
		case XCP_START_STOP_SYNCH:
			Ms_count = 500; break;
		default:
			Ms_count = 500; break;
	}

	SendStatus_ = XCP_S2M_PENDING;
	if (0 != CANSend(GetMasterID(), Length, XCP_Msg)) {
		// printf("callback error!\n");
		return -1;
	}

	while ((SendStatus_ == XCP_S2M_PENDING) && Ms_count) {
		// printf("CCP waiting ACK!!!\n");
		usleep(1000);
		--Ms_count;
	}
	if (Ms_count <= 0) {
		// printf("XCP ACK Timeout!!!\n");
		return -1;
	}
	
	
	if (0xFF != CTO_REPLY_Message_[0]) {
		if (msg_type == XCP_SYNCH) {
			return 0;
		}
		// printf("XCP CTR ERROR!!\n");
		return -1;
	}

	// printf("XCP send OK!!\n");
	return 0;
}

void M2SMessage::Rcv(XCP_ByteVector Msg) {
	// CANRcv();
	// CCPMsgRcv(id, data);
}

void M2SMessage::XCPMsgRcv(XCP_DWORD CAN_ID, XCP_ByteVector XCP_Msg) {
    
	XCP_BYTE PID = XCP_Msg[0];
	switch (PID) {
		case XCP_RES:
            // printf("XCP_RES Rcv\n");
			SendStatus_ = XCP_FREE;
			CTO_REPLY_Message_ = XCP_Msg;
			// std::copy(XCP_Msg.begin(), XCP_Msg.end(), CTO_REPLY_Message_);
			break;
		case XCP_ERR:
			if (XCP_ERR_CMD_SYNCH != XCP_Msg[1]) {
				ERRRcv(XCP_Msg);
			}
			break;
		case XCP_EV:
			break;
		case XCP_SERV:
			break;
		default:
            // printf("DAQRcv\n");
			DAQRcv(CAN_ID, XCP_Msg);
			break;
	}
}

void M2SMessage::ERRRcv(XCP_ByteVector ERR_Msg) {
	XCP_BYTE err_code = ERR_Msg[1];
	switch (err_code) {
		// TODO: invoke error recovery or other services
		case XCP_ERR_CMD_SYNCH:
			break;
		default: break;
	}
}

XCP_RESULT M2SMessage::CMD_CONNECT(XCP_BYTE Mode, XCP_SlaveData& XCP_Slave) {
	
	XCP_BYTE len = 2;

	XCP_BYTE pid = XCP_CONNECT;
	XCP_BYTE mode = Mode;
	XCP_ByteVector cmd_connect = {pid, mode};

	XCP_RESULT ret = XCPMsgSend(cmd_connect, len);
	if (-1 == ret) return -1;
	XCP_Slave.MaxCto = CTO_REPLY_Message_[3];
	if (XCP_Slave.bIntelFormat) {
		XCP_Slave.MaxDto = ((uint16_t)CTO_REPLY_Message_[5] << 8) | CTO_REPLY_Message_[4];
	} else {
		XCP_Slave.MaxDto = ((uint16_t)CTO_REPLY_Message_[4] << 8) | CTO_REPLY_Message_[5];
	}

	return 0;
}

XCP_RESULT M2SMessage::CMD_GET_VERSION() {
	XCP_BYTE len = 2;

	XCP_BYTE pid = XCP_GET_VERSION;
	XCP_BYTE lv1_code = 0x00;
	XCP_ByteVector cmd_get_version = {pid, lv1_code};

	return XCPMsgSend(cmd_get_version, len);
}

XCP_RESULT M2SMessage::CMD_DISCONNECT() {
	XCP_BYTE len = 1;

	XCP_BYTE pid = XCP_DISCONNECT;
	XCP_ByteVector cmd_get_version = {pid};

	return XCPMsgSend(cmd_get_version, len);
}

XCP_RESULT M2SMessage::CMD_GET_STATUS() {
	XCP_BYTE len = 1;

	XCP_BYTE pid = XCP_GET_STATUS;
	XCP_ByteVector cmd_get_status = {pid};

	return XCPMsgSend(cmd_get_status, len);
}

XCP_RESULT M2SMessage::CMD_GET_DAQ_PROCESSOR_INFO(XCP_SlaveData& XCP_Slave) {
	XCP_BYTE len = 1;

	XCP_BYTE pid = XCP_GET_DAQ_PROCESSOR_INFO;
	XCP_ByteVector cmd_free_daq = {pid};
	
	XCP_RESULT ret = XCPMsgSend(cmd_free_daq, len);
	if (-1 == ret) return -1;
	XCP_Slave.DaqProperies.MinDaq = CTO_REPLY_Message_[6];
	XCP_Slave.DaqProperies.IdentificationFieldType = CTO_REPLY_Message_[7];
	XCP_Slave.DaqProperies.TimeStampSupported = CTO_REPLY_Message_[1] & 0x10;
	XCP_Slave.DaqProperies.PidOffSupported = CTO_REPLY_Message_[1] & 0x20;
	XCP_Slave.DaqProperies.PrescalerSupported = CTO_REPLY_Message_[1] & 0x02;
	XCP_Slave.DaqProperies.ConfigType = CTO_REPLY_Message_[1] & 0x01;
	if (XCP_Slave.bIntelFormat) {
		XCP_Slave.DaqProperies.MaxDaq = ((uint16_t)CTO_REPLY_Message_[3] << 8) | CTO_REPLY_Message_[2];
		XCP_Slave.DaqProperies.MaxEventChannel = ((uint16_t)CTO_REPLY_Message_[5] << 8) | CTO_REPLY_Message_[4];
	} else {
		XCP_Slave.DaqProperies.MaxDaq = ((uint16_t)CTO_REPLY_Message_[2] << 8) | CTO_REPLY_Message_[3];
		XCP_Slave.DaqProperies.MaxEventChannel = ((uint16_t)CTO_REPLY_Message_[5] << 4) | CTO_REPLY_Message_[5];
	}
	return 0;
}

XCP_RESULT M2SMessage::CMD_GET_DAQ_RESOLUTION_INFO() {
	XCP_BYTE len = 1;

	XCP_BYTE pid = XCP_GET_DAQ_RESOLUTION_INFO;
	XCP_ByteVector cmd_get_resolution = {pid};
	return XCPMsgSend(cmd_get_resolution, len);
}

XCP_RESULT M2SMessage::CMD_SET_DAQ_LIST_MODE(XCP_BYTE Mode, XCP_WORD DAQ_NO, XCP_WORD Event_Channel_NO, XCP_BYTE Trans_Rate_Pre, XCP_BYTE DAQ_Priority) {
	XCP_BYTE len = 8;

	XCP_BYTE pid = XCP_SET_DAQ_LIST_MODE;
	XCP_BYTE mode = Mode;
	XCP_BYTEPTR p_daq_no = (XCP_BYTEPTR)(&DAQ_NO);
	XCP_BYTEPTR p_event_no = (XCP_BYTEPTR)(&Event_Channel_NO);
	XCP_BYTE prescaler = Trans_Rate_Pre;
	XCP_BYTE priority = DAQ_Priority;

	XCP_RESULT ret = 0;
	if (GetSlaveOrder()) {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, mode, *p_daq_no, *(p_daq_no + 1), *p_event_no, *(p_event_no + 1), prescaler, priority};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	} else {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, mode, *(p_daq_no + 1), *p_daq_no, *(p_event_no + 1), *p_event_no, prescaler, priority};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	}

	if (-1 == ret) return -1;
	return 0;
}

XCP_RESULT M2SMessage::CMD_CLEAR_DAQ_LIST(XCP_WORD DAQ_NO) {
	XCP_BYTE len = 4;

	XCP_BYTE pid = XCP_CLEAR_DAQ_LIST;
	XCP_BYTEPTR p_daq_no = (XCP_BYTEPTR)(&DAQ_NO);

	XCP_RESULT ret = 0;
	if (GetSlaveOrder()) {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, 0x00, *p_daq_no, *(p_daq_no + 1)};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	} else {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, 0x00, *(p_daq_no + 1), *p_daq_no};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	}

	if (-1 == ret) return -1;
	return 0;
}

XCP_RESULT M2SMessage::CMD_FREE_DAQ() {
	XCP_BYTE len = 1;

	XCP_BYTE pid = XCP_FREE_DAQ;
	XCP_ByteVector cmd_free_daq = {pid};
	return XCPMsgSend(cmd_free_daq, len);
}

XCP_RESULT M2SMessage::CMD_ALLOC_DAQ(XCP_WORD DAQ_CNT) {
	XCP_BYTE len = 4;

	XCP_BYTE pid = XCP_ALLOC_DAQ;
	XCP_BYTEPTR p_daq_cnt = (XCP_BYTEPTR)(&DAQ_CNT);

	XCP_RESULT ret = 0;
	if (GetSlaveOrder()) {
		XCP_ByteVector cmd_alloc_daq = {pid, 0x00, *p_daq_cnt, *(p_daq_cnt + 1)};
		ret = XCPMsgSend(cmd_alloc_daq, len);
	} else {
		XCP_ByteVector cmd_alloc_daq = {pid, 0x00, *(p_daq_cnt + 1), *p_daq_cnt};
		ret = XCPMsgSend(cmd_alloc_daq, len);
	}
	return ret;
}
XCP_RESULT M2SMessage::CMD_ALLOC_ODT(XCP_WORD DAQ_NO, XCP_BYTE ODT_CNT) {
	XCP_BYTE len = 5;

	XCP_BYTE pid = XCP_ALLOC_ODT;
	XCP_BYTEPTR p_daq_no = (XCP_BYTEPTR)(&DAQ_NO);
	XCP_BYTE odt_cnt = ODT_CNT;

	XCP_RESULT ret = 0;
	if (GetSlaveOrder()) {
		XCP_ByteVector cmd_alloc_odt = {pid, 0x00, *p_daq_no, *(p_daq_no + 1), odt_cnt};
		ret = XCPMsgSend(cmd_alloc_odt, len);
	} else {
		XCP_ByteVector cmd_alloc_odt = {pid, 0x00, *(p_daq_no + 1), *p_daq_no, odt_cnt};
		ret = XCPMsgSend(cmd_alloc_odt, len);
	}
	return ret;
}

XCP_RESULT M2SMessage::CMD_ALLOC_ODT_ENTRY(XCP_WORD DAQ_NO, XCP_BYTE ODT_NO, XCP_BYTE Element_CNT) {
	XCP_BYTE len = 6;

	XCP_BYTE pid = XCP_ALLOC_ODT_ENTRY;
	XCP_BYTEPTR p_daq_no = (XCP_BYTEPTR)(&DAQ_NO);
	XCP_BYTE odt_no = ODT_NO;
	XCP_BYTE ele_cnt = Element_CNT;

	XCP_RESULT ret = 0;
	if (GetSlaveOrder()) {
		XCP_ByteVector cmd_alloc_odt_entry = {pid, 0x00, *p_daq_no, *(p_daq_no + 1), odt_no, ele_cnt};
		ret = XCPMsgSend(cmd_alloc_odt_entry, len);
	} else {
		XCP_ByteVector cmd_alloc_odt_entry = {pid, 0x00, *(p_daq_no + 1), *p_daq_no, odt_no, ele_cnt};
		ret = XCPMsgSend(cmd_alloc_odt_entry, len);
	}
	return ret;
}


XCP_RESULT M2SMessage::CMD_SET_DAQ_PTR(XCP_WORD DAQ_NO, XCP_BYTE ODT_NO, XCP_BYTE Element_NO) {
	XCP_BYTE len = 6;

	XCP_BYTE pid = XCP_SET_DAQ_PTR;
	XCP_BYTEPTR p_daq_no = (XCP_BYTEPTR)(&DAQ_NO);
	XCP_BYTE odt_no = ODT_NO;
	XCP_BYTE ele_no = Element_NO;

	XCP_RESULT ret = 0;
	if (GetSlaveOrder()) {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, 0x00, *p_daq_no, *(p_daq_no + 1), odt_no, ele_no};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	} else {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, 0x00, *(p_daq_no + 1), *p_daq_no, odt_no, ele_no};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	}

	if (-1 == ret) return -1;
	return 0;
}

// bit_offest
XCP_RESULT M2SMessage::CMD_WRITE_DAQ(XCP_BYTE Size_Element, XCP_BYTE BIT_OFFEST, XCP_BYTE Addr_Ext_Element, XCP_DWORD Addr_Element) {
	XCP_BYTE len = 8;

	XCP_BYTE pid = XCP_WRITE_DAQ;
	XCP_BYTE bit_offest = BIT_OFFEST;
	XCP_BYTE ele_size = Size_Element;
	XCP_BYTE ele_ext_addr = Addr_Ext_Element;
	XCP_BYTEPTR p_ele_addr = (XCP_BYTEPTR)(&Addr_Element);

	XCP_RESULT ret = 0;
	if (GetSlaveOrder()) {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, bit_offest, ele_size, ele_ext_addr, *p_ele_addr, *(p_ele_addr + 1), *(p_ele_addr + 2), *(p_ele_addr + 3)};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	} else {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, bit_offest, ele_size, ele_ext_addr, *(p_ele_addr + 3), *(p_ele_addr + 2), *(p_ele_addr + 1), *p_ele_addr};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	}

	if (-1 == ret) return -1;
	return 0;
}

XCP_RESULT M2SMessage::CMD_START_STOP_DAQ_LIST(XCPStartStopMode Mode, XCP_WORD DAQ_NO) {
	XCP_BYTE len = 4;

	XCP_BYTE pid = XCP_START_STOP_DAQ_LIST;
	XCP_BYTE mode = (XCP_BYTE)Mode;
	XCP_BYTEPTR p_daq_no = (XCP_BYTEPTR)(&DAQ_NO);

	XCP_RESULT ret = 0;
	if (GetSlaveOrder()) {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, mode, *p_daq_no, *(p_daq_no + 1)};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	} else {
		XCP_ByteVector cmd_set_daq_list_mode = {pid, mode, *(p_daq_no + 1), *p_daq_no};
		ret = XCPMsgSend(cmd_set_daq_list_mode, len);
	}

	if (-1 == ret) return -1;
	return 0;	
}

XCP_RESULT M2SMessage::CMD_START_STOP_SYNCH(XCPStartStopSynchMode Mode) {
	XCP_BYTE len = 2;

	XCP_BYTE pid = XCP_START_STOP_SYNCH;
	XCP_BYTE mode = (XCP_BYTE)Mode;

	XCP_ByteVector cmd_start_stop_synch = {pid, mode};
	return XCPMsgSend(cmd_start_stop_synch, len);
}
