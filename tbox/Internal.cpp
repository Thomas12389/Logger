#include <string.h>
#include <stdio.h>

#include "tbox/DevInfo.h"
#include "ConvertData/ConvertData.h"
#include "DevConfig/PhyChannel.hpp"
#include "datatype.h"

#include "Logger/Logger.h"


extern Inside_MAP g_Inside_MAP;

extern stu_OutMessage g_stu_OutMessage;
extern stu_OutMessage g_stu_SaveMessage;

static int runInternalThread;

double Get_VIN(void)
{
    float ad_v_in = 0.0f;
    if (0 == GetAD_V_IN(&ad_v_in)) {
        return ad_v_in;
    }

    return SIGNAL_NAN;
}

double Get_Internal_Tempture(void)
{
    int ad_temp = 0;
    if (0 == GetAD_TEMP(&ad_temp)) {
        return ad_temp;
    }

    return SIGNAL_NAN;
}

double Internal_Signal_Val(std::string& MsgName) {
    if (MsgName == "Tbox_Voltage") {
        return Get_VIN();
    } else if (MsgName == "Tbox_Temperature") {
        return Get_Internal_Tempture();
    }

    return SIGNAL_NAN;
}

void *Internal_Thread(void *arg)
{

    while (runInternalThread) {
        // TODO：200 ms 采集一次
        usleep(200000);
        if (!runInternalThread) break;

        Inside_MAP::iterator ItrSrc = g_Inside_MAP.find("Internal");
        if (ItrSrc == g_Inside_MAP.end()) continue;

        Inside_Message Ins_Msg = ItrSrc->second;
        Inside_SigStruct *pSig = Ins_Msg.pInsideSig;

        for (int idxSig = 0; idxSig < Ins_Msg.nNumSigs; idxSig++) {
            Out_Message Msg = {pSig[idxSig].strOutName, SIGNAL_NAN, "", ""};
            Msg.dPhyVal = Internal_Signal_Val(pSig[idxSig].strSigName);
            Msg.strPhyUnit = pSig[idxSig].strSigUnit;
            Msg.strPhyFormat = pSig[idxSig].strSigFormat;

            // 写输出信号 map
            if (pSig[idxSig].bIsSend) {
                std::lock_guard<std::mutex> lock(g_stu_OutMessage.msg_lock);
                write_msg_out_map(pSig[idxSig].strOutName, Msg);
            }
            // 写存储信号 map
            if (pSig[idxSig].bIsSave) {
                Msg.strName = pSig[idxSig].strSaveName;     // debug for only STORAGE
                std::lock_guard<std::mutex> lock(g_stu_SaveMessage.msg_lock);
                write_msg_save_map(pSig[idxSig].strSaveName, Msg);
            }
        }
    }
    pthread_exit(NULL);

}


int Init_Internal(void)
{
    int retval = 0;
    // Internal 线程
    pthread_t internal_thread_id;
    runInternalThread = 1;
    retval = pthread_create(&internal_thread_id, NULL, Internal_Thread, NULL);
    if (retval) {
        XLOG_DEBUG("Internal pthread_create ERROR.");
        return -1;
    }
    char internal_thread_name[16] = {0};
    strncat(internal_thread_name, "Internal recv", strlen("Internal recv"));
    retval = pthread_setname_np(internal_thread_id, internal_thread_name);
    if (retval) {
        XLOG_DEBUG("Internal pthread_setname_np ERROR.");
    }
    retval = pthread_detach(internal_thread_id);
    if (retval) {
        XLOG_DEBUG("Internal pthread_detach ERROR.");
    }

    return 0;
}

int End_Internal(void)
{
    runInternalThread = 0;

    XLOG_DEBUG("Internal->Module Finalized.");
    return 0;
}
