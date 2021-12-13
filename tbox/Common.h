

#ifndef __COMMON_H
#define __COMMON_H

#include <inttypes.h>
	
#include "owa4x/INET_ModuleDefs.h" 
#include "owa4x/GSM_ModuleDefs.h"
#include "owa4x/pm_messages.h"
#include "owa4x/IOs_ModuleDefs.h"
#include "owa4x/RTUControlDefs.h"
#include "owa4x/owerrors.h"

#define COLOR(a, b)		"\033[" #a "m" b "\033[0m"

#define RED(x)			COLOR(31, x)
#define GREEN(x)		COLOR(32, x)
#define YELLOW(x)		COLOR(33, x)

typedef enum TIME_STAMP : uint8_t {
	S_STAMP,
	MS_STAMP,
	US_STAMP,
	NS_STAMP
} TIME_STAMP;

typedef enum TIMER_ID : uint8_t {
	GPS_TIMER_ID,
	Internal_TIMER_ID,
	PUBLISH_TIMER_ID,
	SAVE_TIMER_ID
} TIMER_ID;

typedef enum COMMUNICATION_MODE : uint8_t {
	LAN,
	WiFi,
	Modem
} COMMUNICATION_MODE;
	
void SyncTime(void);
int InitRTUModule(void);
int EndRTUModule(void);
int InitIOModule(void);
int EndIOModule(void);

// 获取时间戳
uint64_t getTimestamp(TIME_STAMP type);
const char *Str_Error(int code);
void handler_exit(int num);

void pre_to_stop_mode_1();
void pre_to_stop_mode_2();
	
#endif