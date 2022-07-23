
#include "tbox/DevInfo.h"
#include "tbox/Common.h"

#include <stdio.h>

// debug -- 2022.07.20 
/*
#include <sys/sysinfo.h>

int GetAvailableRAM() {
    struct sysinfo s_info;
	int ret = sysinfo(&s_info);
	if (-1 == ret) {
		perror("sysinfo");
		return 1;
	}

	printf("total: %lu, free: %lu\n", s_info.totalram / (1024 * 1024), s_info.freeram / (1024 * 1024));
	return 0;
}
*/
// end debug


int GetBatteryState(unsigned char *BattState)
{
    int iReturnCode = NO_ERROR;

    iReturnCode = RTUGetBatteryState(BattState);
    if (NO_ERROR != iReturnCode) {
        printf("TBox--> ERROR on RTUGetBatteryState\n");
		printf("%s\n",Str_Error(iReturnCode));
		return -1;
    }
    return 0;
}

int GetAD_VBAT_MAIN(float *ad_vbat_main)
{
    int iReturnCode = NO_ERROR;

    iReturnCode = RTUGetAD_VBAT_MAIN(ad_vbat_main);
    if (NO_ERROR != iReturnCode) {
        printf("TBox--> ERROR on RTUGetAD_VBAT_MAIN\n");
		printf("%s\n",Str_Error(iReturnCode));
		return -1;
    }
    return 0;
}

int GetAD_V_IN(float *ad_v_in)
{
    int iReturnCode = NO_ERROR;

    iReturnCode = RTUGetAD_V_IN(ad_v_in);
    if (NO_ERROR != iReturnCode) {
        printf("TBox--> ERROR on RTUGetAD_V_IN\n");
		printf("%s\n",Str_Error(iReturnCode));
		return -1;
    }
    return 0;
}

int GetAD_TEMP(int *ad_temp)
{
    int iReturnCode = NO_ERROR;

    iReturnCode = RTUGetAD_TEMP(ad_temp);
    if (NO_ERROR != iReturnCode) {
        printf("TBox--> ERROR on RTUGetAD_TEMP\n");
		printf("%s\n",Str_Error(iReturnCode));
		return -1;
    }
    return 0;
}

int GetSerial_Number( unsigned char *wSerialNumber)
{
    int iReturnCode = NO_ERROR;

    iReturnCode = GetSerialNumber(wSerialNumber);
    if (NO_ERROR != iReturnCode) {
        printf("TBox--> ERROR on GetSerialNumber\n");
		printf("%s\n",Str_Error(iReturnCode));
		return -1;
    }
    return 0;
}
