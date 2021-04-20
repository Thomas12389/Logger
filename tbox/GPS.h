#ifndef __GPS_H_
#define __GPS_H_

#include "owa4x/GPS2_ModuleDefs.h"
// #include "owa4x/owerrors.h"

typedef struct {
    TUTC_DATE_TIME CurDateTime;
    TGPS_POS CurCoords;
    TGPS_SPEED CurSpeed;
} GPSInfo;

int InitGPSModule(void);
int EndGPSModule(void);

int GetGPSInfo(GPSInfo *spCurGPSinfo);

int GetFullGPSInfo(tPOSITION_DATA *spCurPositionData);

#endif
