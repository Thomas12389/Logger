#include <string.h>
#include <stdio.h>

#include "tbox/Common.h"
#include "tbox/GPS.h"

#include "Logger/Logger.h"

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
    retval = GPS_SetStaticThreshold(10);
    if (NO_ERROR != retval) {
        XLOG_WARN("GPS setting.{}", Str_Error(retval));
        return retval;
    }

    // Set the Navigation/Measurement Rate.
    // retval = GPS_SetMeasurementRate(10);
    // if (NO_ERROR != retval) {
    //     XLOG_WARN("GPS setting.{}", Str_Error(retval));
    //     return retval;
    // }

    // Enable the jamming information feature
    retval = GPS_Configure_ITFM( 1, 3, 15);
    if (NO_ERROR != retval) {
        XLOG_WARN("GPS setting.{}", Str_Error(retval));
        return retval;
    }

    return retval;
}

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
    return 0;
}

int EndGPSModule(void)
{
    int iReturnCode = NO_ERROR, iIsActive = 0;
    GPS_IsActive(&iIsActive);
    if (!iIsActive) return 0;
    if (NO_ERROR != (iReturnCode = GPS_Finalize())) {
        XLOG_DEBUG("TBox--> Error on GPS_Finalize. {}", Str_Error(iReturnCode));
        return -1;
    }
    
    DIGIO_Set_PPS_GPS_Output(); // GPS_Set_Led_Mode(1);
    XLOG_DEBUG("GPS->Module Finalized.");
    return 0;
}

int GetGPSInfo(GPSInfo *spCurGPSinfo)
{
    int iReturnCode = NO_ERROR;

    iReturnCode = GPS_GetPosition_Polling(&(spCurGPSinfo->CurCoords));
    if (NO_ERROR != iReturnCode) {
        printf("TBox--> Error on GPS_GetPosition_Polling\n");
        printf("%s\n",Str_Error(iReturnCode));
        return -1;
    }

    iReturnCode = GPS_GetSpeedCourse_Polling(&(spCurGPSinfo->CurSpeed));
    if (NO_ERROR != iReturnCode) {
        printf("TBox--> Error on GPS_GetSpeedCourse_Polling\n");
        printf("%s\n",Str_Error(iReturnCode));
        return -1;
    }

    iReturnCode = GPS_GetUTCDateTime(&(spCurGPSinfo->CurDateTime));
    if (NO_ERROR != iReturnCode) {
        printf("TBox--> Error on GPS_GetUTCDateTime\n");
        printf("%s\n",Str_Error(iReturnCode));
        return -1;
    }

    return 0;
}

int GetFullGPSInfo(tPOSITION_DATA *spCurPositionData)
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