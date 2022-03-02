
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

#include "IPC.h"

#ifdef USE_MSGQUEUQ

#define MAX_IPCMSG_LEN         8192

static int InitIPCMsgByPath(const char* pPath,int nProjID) {
	int nMsgID = 0;
	//char szTmpBuf[MAX_IPCMSG_LEN]={0};
	key_t MsgKey = ftok(pPath, nProjID);
	if(-1 == MsgKey) return -1;
	nMsgID = msgget(MsgKey, 0666 | IPC_CREAT);
	if(-1 == nMsgID)
	{
		//printf("Get Message ID failed!\n");
		return -1;
	}
	//clear message queue
	//while(msgrcv(nMsgID,szTmpBuf,sizeof(szTmpBuf),0,IPC_NOWAIT) > 0)
	//{
	//	//printf("recv message:%s\n",szTmpBuf);
	//};
	return nMsgID;
}

int InitIPCMsg(Logger_Ctrl *pLogger_Ctrl) {
    
    pLogger_Ctrl->n_logger_to_srv_id = InitIPCMsgByPath(".", IPCMSG_LOGGER_SRV_KEY);
    if(pLogger_Ctrl->n_logger_to_srv_id < 0) {
		// 打印日志
		return -1;
	}

    pLogger_Ctrl->n_srv_to_logger_id = InitIPCMsgByPath(".", IPCMSG_SRV_LOGGER_KEY);
    if(pLogger_Ctrl->n_srv_to_logger_id < 0) {
		// 打印日志
		return -1;
	}

    return 0;
}
int IPCMsgSnd(int MsgID);
int IPCMsgRcv(int MsgID);

int DestroyIPCMsg(int nMsgID) {
	return	msgctl(nMsgID, IPC_RMID, 0);
}

int SendIPCMsg(int nMsgID, int nLen, void* pMsg) {
	return msgsnd(nMsgID, pMsg, nLen, 0);
}

int RecvIPCMsg(int nMsgID, int nLen, void* pMsg, int bWait) {

	if(bWait) {
		return msgrcv(nMsgID, pMsg, nLen, 0, 0);
	} else {
		return msgrcv(nMsgID, pMsg, nLen, 0, IPC_NOWAIT);
	}
}

int RecvIPCMsgTimeout(int nMsgID, int nLen, void* pMsg, unsigned long mSec) {
	int nRet = 0;
	unsigned long nIdx = 0;
	nRet = msgrcv(nMsgID, pMsg, nLen, 0, IPC_NOWAIT);
	if(nRet > 0) return nRet;

	for(nIdx = 0; nIdx < mSec; nIdx++) {
		struct timeval tv = {0, 1};//1ms
		select(0, NULL, NULL, NULL, &tv);
		nRet = msgrcv(nMsgID,pMsg,nLen,0,IPC_NOWAIT);
		if(nRet > 0) return nRet;
	}
	return -1;
}

void ClsIPCMsg(int nMsgID) {
	char szTmpBuf[MAX_IPCMSG_LEN] = {0};
	while(msgrcv(nMsgID, szTmpBuf, sizeof(szTmpBuf), 0, IPC_NOWAIT) > 0);
}

#else

static int InitIPCFIFOByPath(const char* pPath) {
	int ret = 0;
	//char szTmpBuf[MAX_IPCMSG_LEN]={0};
    ret = access(pPath, F_OK);
    if(-1 == ret) {
        ret = mkfifo(pPath, 0666);
        if (ret < 0) {
            perror("mkfifo");
            return -1;
        }
    }

    ret = open(pPath, O_RDWR);
    if (-1 == ret) {
        perror("open");
        return -1;
    }

	return ret;
}

int InitIPCFIFO(Logger_Ctrl *pLogger_Ctrl) {
    pLogger_Ctrl->n_logger_to_srv_fd = InitIPCFIFOByPath(FIFO_RSP_PATH);
    if (pLogger_Ctrl->n_logger_to_srv_fd < 0 ) {
        // 打印日志
        return -1;
    }
    pLogger_Ctrl->n_srv_to_logger_fd = InitIPCFIFOByPath(FIFO_REQ_PATH);
    if (pLogger_Ctrl->n_logger_to_srv_fd < 0 ) {
        // 打印日志
        return -1;
    }
    return 0;
}

int IPCFIFOSnd(int s_fd) {
    return 0;
}

int IPCFIFORcv(int r_fd) {

    return 0;
}

#endif
