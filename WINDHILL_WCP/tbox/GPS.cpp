#include <string.h>
#include <stdio.h>
#include <semaphore.h>

#include "tbox/Common.h"
#include "tbox/GPS.h"
#include "ConvertData/ConvertData.h"
#include "DevConfig/PhyChannel.hpp"
#include "datatype.h"

#include "Logger/Logger.h"

extern Inside_MAP g_Inside_MAP;

extern stu_OutMessage g_stu_OutMessage;
extern stu_OutMessage g_stu_SaveMessage;

static int runGPSThread;

#define GPS_TIMER

#ifdef GPS_TIMER
static uint8_t GPS_Timer_id = TIMER_ID::GPS_TIMER_ID;
static sem_t g_sem_gps;
#endif

typedef struct {
    tPOSITION_DATA CurFullPostion;
    TUTC_DATE_TIME CurDateTime;
    tGSV_Data CurSVStatus;
} GPSInfo;

int GetFullGPSPosition(tPOSITION_DATA *spCurPositionData)
{
    int iReturnCode = NO_ERROR;

    iReturnCode = GPS_GetAllPositionData(spCurPositionData);
    if (NO_ERROR != iReturnCode) {
        printf("TBox--> Error on GPS_GetAllPositionData\n");
        printf("%s\n",Str_Error(iReturnCode));
        return -1;
    }
    return 0;
}

int GetUTCDateTime(TUTC_DATE_TIME *pCurDateTime)
{
    int iReturnCode = NO_ERROR;
    
    iReturnCode = GPS_GetUTCDateTime(pCurDateTime);
    if( iReturnCode != NO_ERROR ) {
        printf("TBox--> Error on GPS_GetUTCDateTime\n");
        printf("%s\n",Str_Error(iReturnCode));
        return -1;
    }

    return 0;
}

int GetSVStatus(tGSV_Data *pData) {
    int iReturnCode = NO_ERROR;
    
    iReturnCode = GPS_GetSV_inView(pData);
    if( iReturnCode != NO_ERROR ) {
        printf("TBox--> Error on GPS_GetSV_inView\n");
        printf("%s\n",Str_Error(iReturnCode));
        return -1;
    }

    return 0;
}

double GPS_Signal_Val(std::string& MsgName, GPSInfo *sCurGPSInfo) {
    if (MsgName == "OWA_Status") {
        return sCurGPSInfo->CurFullPostion.PosValid;
    } else if (MsgName == "OWA_numSvs") {
        return sCurGPSInfo->CurFullPostion.numSvs;
    } else if (MsgName == "OWA_Altitude") {
        return sCurGPSInfo->CurFullPostion.Altitude;
    } else if (MsgName == "OWA_Course") {
        return sCurGPSInfo->CurFullPostion.Course;
    } else if (MsgName == "OWA_Longitude") {
        return sCurGPSInfo->CurFullPostion.LonDecimal;
    } else if (MsgName == "OWA_Latitude") {
        return sCurGPSInfo->CurFullPostion.LatDecimal;
    } else if (MsgName == "OWA_Speed") {
        return sCurGPSInfo->CurFullPostion.Speed;
    } else if (MsgName == "OWA_UTC_Year") {
        return sCurGPSInfo->CurDateTime.Year;
    } else if (MsgName == "OWA_UTC_Month") {
        return sCurGPSInfo->CurDateTime.Month;
    } else if (MsgName == "OWA_UTC_Day") {
        return sCurGPSInfo->CurDateTime.Day;
    } else if (MsgName == "OWA_UTC_Hour") {
        return sCurGPSInfo->CurDateTime.Hours;
    } else if (MsgName == "OWA_UTC_Minute") {
        return sCurGPSInfo->CurDateTime.Minutes;
    } else if (MsgName == "OWA_UTC_Second") {
        return (uint16_t)(sCurGPSInfo->CurDateTime.Seconds);
    } else if (MsgName == "OWA_UTC_MilliSecond") {
        return (sCurGPSInfo->CurDateTime.Seconds * 1000 / 1000);
    } else if (MsgName == "OWA_Visible_sats") {
        return sCurGPSInfo->CurSVStatus.SV_InView;
    }

    return SIGNAL_NAN;
}

void *GPS_Thread(void *arg)
{
    (void)arg;
    GPSInfo sCurGPSInfo;

    while (runGPSThread) {
#ifdef GPS_TIMER
        sem_wait(&g_sem_gps);
#else
        // 50ms
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
#endif
        if (!runGPSThread) break;

        Inside_MAP::iterator ItrSrc = g_Inside_MAP.find("GPS");
        if (ItrSrc == g_Inside_MAP.end()) continue;

        GetFullGPSPosition(&sCurGPSInfo.CurFullPostion);
        GetUTCDateTime(&sCurGPSInfo.CurDateTime);
        GetSVStatus(&sCurGPSInfo.CurSVStatus);

        Inside_Message Ins_Msg = ItrSrc->second;
        Inside_SigStruct *pSig = Ins_Msg.pInsideSig;

        for (int idxSig = 0; idxSig < Ins_Msg.nNumSigs; idxSig++) {
            Out_Message Msg = {pSig[idxSig].strOutName, SIGNAL_NAN, "", ""};
            
            Msg.dPhyVal = GPS_Signal_Val(pSig[idxSig].strSigName, &sCurGPSInfo);
            Msg.strPhyUnit = pSig[idxSig].strSigUnit;
            Msg.strPhyFormat = pSig[idxSig].strSigFormat;

            // 数据未发生变化
            if (sCurGPSInfo.CurFullPostion.OldValue) {
                continue;
            }

            // 定位卫星过少或状态不可用时，只重写状态和数量
            if ( (sCurGPSInfo.CurFullPostion.numSvs < 4) || 
                (sCurGPSInfo.CurFullPostion.PosValid == 0) ){
                
                if ("OWA_Status" != pSig[idxSig].strSigName && 
                    "OWA_numSvs" != pSig[idxSig].strSigName) {

                    if (pSig[idxSig].bIsSend) {
                        std::lock_guard<std::mutex> lock(g_stu_OutMessage.msg_lock);
                        write_msg_out_map(pSig[idxSig].strOutName, {pSig[idxSig].strOutName, SIGNAL_NAN, "", ""});
                    }
                    if (pSig[idxSig].bIsSave) {
                        Msg.strName = pSig[idxSig].strSaveName;
                        std::lock_guard<std::mutex> lock(g_stu_SaveMessage.msg_lock);
                        write_msg_save_map(pSig[idxSig].strSaveName, {pSig[idxSig].strSaveName, SIGNAL_NAN, "", ""});
                    }
                        
                    continue;
                }
            }
            
            // 写输出信号 map
            if (pSig[idxSig].bIsSend) {
                std::lock_guard<std::mutex> lock(g_stu_OutMessage.msg_lock);
                write_msg_out_map(pSig[idxSig].strOutName, Msg);
            }
            // 写存储信号 map
            if (pSig[idxSig].bIsSave) {
                Msg.strName = pSig[idxSig].strSaveName;
                std::lock_guard<std::mutex> lock(g_stu_SaveMessage.msg_lock);
                write_msg_save_map(pSig[idxSig].strSaveName, Msg);
            }
        }
    }
    pthread_exit(NULL);

}


int GPS_Setting(void) {
    int retval = NO_ERROR;
    // Set the setting for the dynamic platform model in the GPS receiver.
    // 4 Automotive
    retval = GPS_SetDynamicModel(4);
    if (NO_ERROR != retval) {
        XLOG_WARN("GPS setting.{}", Str_Error(retval));
        return retval;
    }

    // Set the speed threshold for gps navigation solution.
    // unit: cm/s
    // 普遍适用范围： 25 - 50
    retval = GPS_SetStaticThreshold(25);
    if (NO_ERROR != retval) {
        XLOG_WARN("GPS setting.{}", Str_Error(retval));
        return retval;
    }

    // Set the Navigation/Measurement Rate.
    retval = GPS_SetMeasurementRate(4);
    if (NO_ERROR != retval) {
        XLOG_WARN("GPS setting.{}", Str_Error(retval));
        return retval;
    }

    // Enable the jamming information feature
    retval = GPS_Configure_ITFM( 1, 3, 15);
    if (NO_ERROR != retval) {
        XLOG_WARN("GPS setting.{}", Str_Error(retval));
        return retval;
    }

    return retval;
}

#ifdef GPS_TIMER
void gps_handler() {
    sem_post(&g_sem_gps);
    return ;
}
#endif

int InitGPSModule(void)
{
    TGPS_MODULE_CONFIGURATION GPSConfiguration;
    char GpsValidType[][20] = {"NONE", "GPS_UBLOX"};
    char GpsValidProtocol[][10] = {"NMEA", "BINARY"};
    int iReturnCode = NO_ERROR, iIsActive = 0;

    memset(&GPSConfiguration, 0, sizeof(TGPS_MODULE_CONFIGURATION));
    strcpy(GPSConfiguration.DeviceReceiverName, GpsValidType[1]);
    GPSConfiguration.ParamBaud = B115200;
    GPSConfiguration.ParamLength = CS8;
    GPSConfiguration.ParamParity = IGNPAR;
    strcpy(GPSConfiguration.ProtocolName, GpsValidProtocol[0]);
    GPSConfiguration.GPSPort = COM1;

    // GPS module initialization
    if (NO_ERROR != (iReturnCode = GPS_Initialize((void *)&GPSConfiguration))) {
        if (ERROR_GPS_ALREADY_INITIALIZED != iReturnCode) {	//Error 244: GSM Module already initialized.
            XLOG_DEBUG("TBox--> ERROR on GPS_Initialize. {}", Str_Error(iReturnCode));
			return -1;
		}
    }

    // GPS receiver startup
    if (NO_ERROR != (iReturnCode = GPS_Start())) {
        if (ERROR_GPS_ALREADY_STARTED != iReturnCode) {
            XLOG_DEBUG("TBox--> ERROR on GPS_Start. {}", Str_Error(iReturnCode));
			return -1;
        }
    }

    if (NO_ERROR != (iReturnCode = GPS_Setting())) {
        XLOG_DEBUG("TBox--> ERROR on GPS_Setting. {}", Str_Error(iReturnCode));
    }

    XLOG_DEBUG("TBox--> OK on GPS InitGPSMoudle.");
    GPS_IsActive(&iIsActive);
    if (iIsActive) {
        DIGIO_Set_PPS_GPS_Input();   // GPS_Set_Led_Mode(0);
    };

    XLOG_DEBUG("TBox--> GPS Moudle is active {}.", iIsActive ? "OK" : "FAIL");

#ifdef GPS_TIMER
    sem_init(&g_sem_gps, 0, 0);
#endif

    // GPS 线程
    pthread_t gps_thread_id;
    runGPSThread = 1;
    pthread_create(&gps_thread_id, NULL, GPS_Thread, NULL);
    char gps_thread_name[16] = {0};
    strncat(gps_thread_name, "GPS recv", strlen("GPS recv"));
    pthread_setname_np(gps_thread_id, gps_thread_name);
    pthread_detach(gps_thread_id);

#ifdef GPS_TIMER
    // TODO: 优化显示效果

    // clock_t start = clock();
    // tPOSITION_DATA Postion;
    // while (GPS_GetAllPositionData(&Postion), !Postion.PosValid);
    // clock_t end = clock();

    TUTC_DATE_TIME temp;
    while (GPS_GetUTCDateTime(&temp), (int)(temp.Seconds * 1000) % 1000 > 200 );
    // clock_t end2 = clock();
    // XLOG_TRACE("The time taken to wait GPS {:.3f} ms.\n", 1000.0 * ( end - start) / CLOCKS_PER_SEC);
    // XLOG_TRACE("The time taken to wait GPS time {:.3f} ms.\n", 1000.0 * ( end2 - end) / CLOCKS_PER_SEC);
    // XLOG_TRACE("GPS second = {:.f}\n", temp.Seconds);
    
    // 定时器
    uint64_t delayMs = g_Inside_MAP["GPS"].nSampleCycleMs;
    OWASYS_GetTimer(&GPS_Timer_id, (void (*)(unsigned char)) & gps_handler, delayMs / 1000, (delayMs % 1000) * 1000);
    OWASYS_StartTimer(GPS_Timer_id, MULTIPLE_TICK);
#endif

    // char rate = 0;
    // int retval = GPS_GetMeasurementRate(&rate);
    // if (NO_ERROR == retval) {
    //     printf("GPS MeasurementRate: %d\n", rate);
    // }

    return 0;
}

int EndGPSModule(void)
{
    int iReturnCode = NO_ERROR, iIsActive = 0;
    runGPSThread = 0;

#ifdef GPS_TIMER
    OWASYS_StopTimer(TIMER_ID::GPS_TIMER_ID);
    OWASYS_FreeTimer(TIMER_ID::GPS_TIMER_ID);
#endif

    GPS_IsActive(&iIsActive);
    if (!iIsActive) return 0;

    if (NO_ERROR != (iReturnCode = GPS_Finalize())) {
        XLOG_DEBUG("Error on GPS_Finalize. {}", Str_Error(iReturnCode));
        return -1;
    }
    
    DIGIO_Set_PPS_GPS_Output(); // GPS_Set_Led_Mode(1);
    XLOG_DEBUG("GPS->Module Finalized.");
    return 0;
}
