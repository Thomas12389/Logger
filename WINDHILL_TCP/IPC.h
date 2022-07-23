#ifndef __IPC_H__
#define __IPC_H__

#include <inttypes.h>

// #define USE_MSGQUEUQ

#ifdef USE_MSGQUEUQ
    #define IPCMSG_SRV_LOGGER_KEY   0x0A
    #define IPCMSG_LOGGER_SRV_KEY   0x0B
#else
    #define FIFO_REQ_PATH "/tmp/LoggerRequest"
    #define FIFO_RSP_PATH "/tmp/LoggerResponse"
#endif

typedef struct REQUEST_MSG {
    uint16_t m_act;
} Req_Msg;

typedef struct RESPONSE_MSG {
    uint16_t m_rsp;
} Rsp_Msg;


#ifdef USE_MSGQUEUQ

typedef struct LOGGER_CTRL {
    int n_srv_to_logger_id;
    int n_logger_to_srv_id;
} Logger_Ctrl;

int InitIPCMsg(Logger_Ctrl *pLogger_Ctrl);
int IPCMsgSnd(int MsgID);
int IPCMsgRcv(int MsgID);

#else

typedef struct LOGGER_CTRL {
    int n_srv_to_logger_fd;
    int n_logger_to_srv_fd;
} Logger_Ctrl;

int InitIPCFIFO(Logger_Ctrl *pLogger_Ctrl);
int IPCFIFOSnd(int s_fd);
int IPCFIFORcv(int r_fd);

#endif


#endif