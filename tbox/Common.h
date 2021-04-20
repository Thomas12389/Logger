

#ifndef __COMMON_H
	#define __COMMON_H

	#include <inttypes.h>
	
	#include "owa4x/GSM_ModuleDefs.h"
	#include "owa4x/pm_messages.h"
	#include "owa4x/IOs_ModuleDefs.h"
	#include "owa4x/RTUControlDefs.h"
	#include "owa4x/owerrors.h"
	#include "owa4x/INET_ModuleDefs.h" 

	#define COLOR(a, b)		"\033[" #a "m" b "\033[0m"

	#define RED(x)			COLOR(31, x)
	#define GREEN(x)		COLOR(32, x)
	#define YELLOW(x)		COLOR(33, x)
	
	void GetTime(void);
	int InitRTUModule(void);
	int EndRTUModule(void);
	int InitIOModule(void);
	int EndIOModule(void);

	// 获取时间戳
	// type: 0 -- s, 1 -- ms, 2-- us, 3 -- ns
	uint64_t getTimestamp(int type);
	const char *Str_Error(int code);
	void handler_exit(int num);

	void pre_to_stop_mode_1();
	void pre_to_stop_mode_2();
	
#endif