#ifndef __DEVINFO_H_
#define __DEVINFO_H_


int GetBatteryState(unsigned char *BattState);

int GetAD_VBAT_MAIN(float *ad_vbat_main);

int GetAD_V_IN(float *ad_v_in);

int GetAD_TEMP(int *ad_temp);

int GetSerial_Number( unsigned char *wSerialNumber);

#endif