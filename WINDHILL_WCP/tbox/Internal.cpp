#include <string.h>
#include <stdio.h>
#include <semaphore.h>

#include "tbox/Common.h"
#include "tbox/DevInfo.h"
#include "ConvertData/ConvertData.h"
#include "DevConfig/PhyChannel.hpp"
#include "datatype.h"

#include "Logger/Logger.h"

#define INTERNAL_TIMER

#ifdef INTERNAL_TIMER
static uint8_t INTERNAL_Timer_id = TIMER_ID::Internal_TIMER_ID;
static sem_t g_sem_internal;
#endif

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
    if (MsgName == "OWA_Power") {
        return Get_VIN();
    } else if (MsgName == "OWA_Temperature") {
        return Get_Internal_Tempture();
    } 
    // TODO: 获取 RAM 及 SD 可用大小
    else if (MsgName == "OWA_AvailableRamMB") {

    } else if (MsgName == "OWA_AvailableSDMB") {

    }

    return SIGNAL_NAN;
}

#ifdef INTERNAL_TIMER
void internal_handler() {
    sem_post(&g_sem_internal);
    return ;
}
#endif

void *Internal_Thread(void *arg)
{

    while (runInternalThread) {
#ifdef INTERNAL_TIMER
        sem_wait(&g_sem_internal);
#else
        // 50ms
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
#endif

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

#ifdef INTERNAL_TIMER
    sem_init(&g_sem_internal, 0, 0);
#endif

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

#ifdef INTERNAL_TIMER
    // 定时器
    uint64_t delayMs = g_Inside_MAP["Internal"].nSampleCycleMs;
    OWASYS_GetTimer(&INTERNAL_Timer_id, (void (*)(unsigned char)) & internal_handler, delayMs / 1000, (delayMs % 1000) * 1000);
    OWASYS_StartTimer(INTERNAL_Timer_id, MULTIPLE_TICK);
#endif

    return 0;
}

int End_Internal(void)
{
    runInternalThread = 0;

#ifdef INTERNAL_TIMER
    OWASYS_StopTimer(TIMER_ID::Internal_TIMER_ID);
    OWASYS_FreeTimer(TIMER_ID::Internal_TIMER_ID);
#endif

    XLOG_DEBUG("Internal->Module Finalized.");
    return 0;
}
