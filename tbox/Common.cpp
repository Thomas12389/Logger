
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "tbox/Common.h"
#include "tbox/GSM.h"
#include "tbox/GPRS_iNet.h"
#include "tbox/GPS.h"
#include "tbox/CAN.h"
#include "Mosquitto/MOSQUITTO.h"

#include "Logger/Logger.h"

char error[255] = {0};

extern int OBD_Run;
extern int CCP_Run;

void GetTime(void)
{
	int ReturnCode = NO_ERROR;
	TSYSTEM_TIME Sys_Time;
    
	if(NO_ERROR != (ReturnCode = GetSystemTime(&Sys_Time)))
	{
		printf("%s\n",Str_Error(ReturnCode));
		return;
	}
	printf("Date %04lu-%02u-%02u  Time %02u:%02u:%02u\n",
		Sys_Time.Year,Sys_Time.Month,Sys_Time.Day,Sys_Time.Hour,Sys_Time.Minute,Sys_Time.Second);
}

void SyncTime(void)
{
	int ReturnCode = NO_ERROR;
	TSYSTEM_TIME Sys_Time;
    
	if(NO_ERROR != (ReturnCode = GetSystemTime(&Sys_Time)))
	{
		XLOG_WARN("%s",Str_Error(ReturnCode));
		return;
	}
	// printf("Date %04lu-%02u-%02u  Time %02u:%02u:%02u\n",
	// 	Sys_Time.Year,Sys_Time.Month,Sys_Time.Day,Sys_Time.Hour,Sys_Time.Minute,Sys_Time.Second);

	THW_TIME_DATE curtime;
	curtime.year = Sys_Time.Year;
	curtime.month = Sys_Time.Month;
	curtime.day = Sys_Time.Day;
	curtime.hour = Sys_Time.Hour - 8; // 东八区
	curtime.min = Sys_Time.Minute;
	curtime.sec = Sys_Time.Second;
	ReturnCode = RTUSetHWTime(curtime);
	
	if (NO_ERROR != ReturnCode) {
		XLOG_WARN("Calibration time failed:{}",Str_Error(ReturnCode));
		return;
	}
	XLOG_INFO("Calibration time: {:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d} (UTC)",
		curtime.year, curtime.month, curtime.day, curtime.hour, curtime.min, curtime.sec);
}

int InitRTUModule(void)
{
	int ReturnCode = NO_ERROR;
	struct stat buf;
	// wait some seconds until owaapi.lck is removed from thr filesystem
	for (int i = 0; i < 15; ++i) {
		if (-1 == (ReturnCode = stat("/var/lock/owaapi.lck", &buf))) {
			i = 15;
		}
		usecsleep(1, 0);
	}
	if (0 == ReturnCode) {
		XLOG_DEBUG("/var/lock/owaapi.lck is always exist.");
		return 1;
	}

	// start RTUControl
	if( NO_ERROR != (ReturnCode = RTUControl_Initialize(NULL)) )
	{
		if(ERROR_RTUCONTROL_ALREADY_INITALIZED != ReturnCode)	//Error 23: RTU Control already initialized.
		{
			XLOG_DEBUG("ERROR in RTUControl_Initialize. {}",Str_Error(ReturnCode));
			return -1;
		}
	}

	if( NO_ERROR != (ReturnCode = RTUControl_Start()) )
	{
		if(ERROR_RTUCONTROL_ALREADY_STARTED != ReturnCode)	//Error 25: RTU Control already started.
		{
			XLOG_DEBUG("RROR in RTUControl_Start. {}",Str_Error(ReturnCode));
			return -1;
		}
  	}
	
	return 0;
}

int EndRTUModule( void )
{
	int ReturnCode = NO_ERROR;
	if(NO_ERROR != (ReturnCode = RTUControl_Finalize( ))) {
		XLOG_DEBUG("{}",Str_Error(ReturnCode));
		return -1;
	} 
	
	XLOG_INFO("RTU Module Finalized!");

	return 0;
}

int InitIOModule( void )
{
	int ReturnCode = NO_ERROR; 
  	// configure and start IOsModule
	if(NO_ERROR != (ReturnCode = IO_Initialize()) )
	{
		if(ERROR_IOS_ALREADY_INITIALIZED != ReturnCode)	//Error 114: IOs Module already initialized.
		{
			XLOG_DEBUG("{}",Str_Error(ReturnCode));
			return -1;
		}    
	}
	
 	if(NO_ERROR != (ReturnCode = IO_Start()) )
	{
		if(ERROR_IOS_ALREADY_STARTED != ReturnCode)	//Error 116: IOs Module already started.
		{
			XLOG_DEBUG("{}",Str_Error(ReturnCode));
			return -1;
		}
	}
	
	return 0;
}

int EndIOModule( void )
{
	int ReturnCode = NO_ERROR;
   	if(NO_ERROR != (ReturnCode = IO_Finalize())) {
		XLOG_DEBUG("{}",Str_Error(ReturnCode));
		return -1;
	}

	XLOG_INFO("IO Module Finalized!");
	return 0;
}

void handler_exit(int num) {
	unsigned char id = 1;
	OWASYS_StopTimer(id);

	int ReturnCode = 0;
	// the GPS operation must be finish in case other modules remain running
	ReturnCode = EndGPSModule();
	if (-1 == ReturnCode) {
		fprintf(stderr, "EndGPSModule ERROR!\n");
	}

	OBD_Run = 0;
	CCP_Run = 0;
	Can_Stop();
	
	ReturnCode = Mosquitto_Disconnect();
	if (-1 == ReturnCode) {
		fprintf(stderr, "Mosquitto_Disconnect ERROR!\n");
	}

	SyncTime();

	iNetStop();
	
	gsmStop();	//Switches OFF the GSM module and ends all GSM communication

	ReturnCode = EndIOModule();
	if (-1 == ReturnCode) {
		fprintf(stderr, "EndIOModule ERROR!\n");
	}

	ReturnCode = EndRTUModule();
	if (-1 == ReturnCode) {
		fprintf(stderr, "EndRTUModule ERROR!\n");
	}

	XLOG_END();
	
	exit(num);
}

void pre_to_stop_mode_1() {

	int ReturnCode = 0;
	// the GPS operation must be finish in case other modules remain running
	ReturnCode = EndGPSModule();
	if (-1 == ReturnCode) {
		XLOG_WARN("End GPS module ERROR.");
	} else {
		XLOG_INFO("End GPS module successfully.");
	}

	OBD_Run = 0;
	CCP_Run = 0;
	Can_Stop();

	return ;
}

void pre_to_stop_mode_2() {
	
	SyncTime();

	iNetStop();
	
	gsmStop();	//Switches OFF the GSM module and ends all GSM communication

	return ;
}

// get the timestamp
uint64_t getTimestamp(int type) {

	struct timespec time_ns;
	clock_gettime(CLOCK_REALTIME, &time_ns);
	uint64_t time_stamp = 0;
	switch (type) {
		case 1:	{	// ms
			time_stamp = (uint64_t)time_ns.tv_sec * 1000 + time_ns.tv_nsec / 1000000;
			break;
		}
		case 2: {	// us
			time_stamp = (uint64_t)time_ns.tv_sec * 1000000 + time_ns.tv_nsec / 1000;
			break;
		}
		case 3: {	// ns
			time_stamp = (uint64_t)time_ns.tv_sec * 1000000000 + time_ns.tv_nsec;
			break;
		}
		default: {
			time_stamp = (uint64_t)time_ns.tv_sec;
			break;
		}
	}

    return time_stamp;
}

const char *Str_Error(int code)
{
	memset(&error,0,sizeof(error));
	switch(code)
	{
		case 0:sprintf(error,"%s",">>> All OK. <<<");break;
		case 1:sprintf(error,"%s",">>> Error 1: Function not yet implemented. <<<");break;
		case 2:sprintf(error,"%s",">>> Error 2: Non-Valid Parameters. <<<");break;
		/* RTUControl error */
		case 3:sprintf(error,"%s",">>> Error 3: Error when opening port. <<<");break;
		case 4:sprintf(error,"%s",">>> Error 4: Error when closing port. <<<");break;
		case 5:sprintf(error,"%s",">>> Error 5: Insuficient memory for creating Port node. <<<");break;
		case 6:sprintf(error,"%s",">>> Error 6: Invalid status of port. <<<");break;
		case 7:sprintf(error,"%s",">>> Error 7: No valid port node passed to function. <<<");break;
		case 8:sprintf(error,"%s",">>> Error 8: Port already inserted in Select list. <<<");break;
		case 9:sprintf(error,"%s",">>> Error 9: Error when inserting Port node in Select list. <<<");break;
		case 10:sprintf(error,"%s",">>> Error 10: Port not inserted in Select list. <<<");break;
		case 11:sprintf(error,"%s",">>> Error 11: Port not closed when trying to delete from Select list. <<<");break;
		case 12:sprintf(error,"%s",">>> Error 12: Invalid parameter passed to function. <<<");break;
		case 13:sprintf(error,"%s",">>> Error 13: Select Control loop already running. <<<");break;
		case 14:sprintf(error,"%s",">>> Error 14: Select control loop not running. <<<");break;
		case 15:sprintf(error,"%s",">>> Error 15: Impossible to create Select control loop thread. <<<");break;
		case 16:sprintf(error,"%s",">>> Error 16: Timeout reached when closing Select control loop thread. <<<");break;
		case 17:sprintf(error,"%s",">>> Error 17: Impossible to close Select control loop thread. <<<");break;
		case 18:sprintf(error,"%s",">>> Error 18: Port not open. <<<");break;
		case 19:sprintf(error,"%s",">>> Error 19: Invalid file descriptor assigned to port. <<<");break;
		case 20:sprintf(error,"%s",">>> Error 20: No available memory for a reservation. <<<");break;
		case 21:sprintf(error,"%s",">>> Error 21: Last free signal obtained. <<<");break;
		case 22:sprintf(error,"%s",">>> Error 22: No free signals are available in the system. <<<");break;
		case 23:sprintf(error,"%s",">>> Error 23: RTU Control already initialized. <<<");break;
		case 24:sprintf(error,"%s",">>> Error 24: RTU Control not yet initialized. <<<");break;
		case 25:sprintf(error,"%s",">>> Error 25: RTU Control already started. <<<");break;
		case 26:sprintf(error,"%s",">>> Error 26: RTU Control not yet started. <<<");break;
		case 27:sprintf(error,"%s",">>> Error 27: Error when opening OWA22X_M driver. <<<");break;
		case 28:sprintf(error,"%s",">>> Error 28: Error when closing OWA22X_M driver. <<<");break;
		case 29:sprintf(error,"%s",">>> Error 29: OWA22X_M driver already opened. <<<");break;
		case 30:sprintf(error,"%s",">>> Error 30: OWA22X_M driver not opened. <<<");break;
		case 31:sprintf(error,"%s",">>> Error 31: Error when executing a device driver IOCTL command. <<<");break;
		case 32:sprintf(error,"%s",">>> Error 32: Error in file type. <<<");break;
		case 33:sprintf(error,"%s",">>> Error 33: Error in file name. <<<");break;
		case 34:sprintf(error,"%s",">>> Error 34: Error in BootLoader Header. <<<");break;
		case 35:sprintf(error,"%s",">>> Error 35: Error Opening PM driver. <<<");break;
		case 36:sprintf(error,"%s",">>> Error 36: Error Closing PM driver. <<<");break;
		case 37:sprintf(error,"%s",">>> Error 37: Error Calling PM driver. <<<");break;
		case 38:sprintf(error,"%s",">>> Error 38: Error calling usecsleep function. <<<");break;
		case 39:sprintf(error,"%s",">>> Error 39: Error when opening owa3x_fd driver. <<<");break;
		case 40:sprintf(error,"%s",">>> Error 40: Error when closing owa3x_fd driver. <<<");break;
		case 41:sprintf(error,"%s",">>> Error 41: owa3x_fd driver already opened. <<<");break;
		case 42:sprintf(error,"%s",">>> Error 42: owa3x_fd driver not opened. <<<");break;
		case 43:sprintf(error,"%s",">>> Error 43: Communication with the power managemnet unit not started. <<<");break;
		case 44:sprintf(error,"%s",">>> Error 44: Not answer from power management unit. <<<");break;
		case 45:sprintf(error,"%s",">>> Error 45: owa3x_pm driver not opened. <<<");break;
		case 46:sprintf(error,"%s",">>> Error 46: Impossible to read Digital Input. <<<");break;
		case 47:sprintf(error,"%s",">>> Error 47: Impossible to set Digital Output. <<<");break;
		case 48:sprintf(error,"%s",">>> Error 48: Impossible to config interrup for input. <<<");break;
		case 49:sprintf(error,"%s",">>> Error 49: Error in input number. <<<");break;
		case 50:sprintf(error,"%s",">>> Error 50: Error in Output number. <<<");break;
		case 51:sprintf(error,"%s",">>> Error 51: Input already configured. <<<");break;
		case 52:sprintf(error,"%s",">>> Error 52: Interrupt not enabled. <<<");break;
		case 53:sprintf(error,"%s",">>> Error 53: Error in PD mask. <<<");break;
		case 54:sprintf(error,"%s",">>> Error 54: Error in Output Value. <<<");break;
		case 55:sprintf(error,"%s",">>> Error 55: Communication with the expansion board not possible. <<<");break;
		case 56:sprintf(error,"%s",">>> Error 56: Not answer from expansion board unit. <<<");break;
		case 57:sprintf(error,"%s",">>> Error 57: iButton not detected. <<<");break;
		case 58:sprintf(error,"%s",">>> Error 58: error umount /home. <<<");break;
		case 59:sprintf(error,"%s",">>> Error 59: error mount /home. <<<");break;
		case 60:sprintf(error,"%s",">>> Error 60: NOT_USED. <<<");break;
		case 61:sprintf(error,"%s",">>> Error 61: Acceleration not available. <<<");break;
		case 62:sprintf(error,"%s",">>> Error 62: Accelerometer already started. <<<");break;
		case 63:sprintf(error,"%s",">>> Error 63: Error in PM answer. <<<");break;
		case 64:sprintf(error,"%s",">>> Error 64: Error Entering Stand-By. <<<");break;
		case 65:sprintf(error,"%s",">>> Error 65: Error low battery voltage or battery not present. <<<");break;
		/* IOs error */
		case 101:sprintf(error,"%s",">>> Error 101: Trying to use a Digital output as PWM, or viceverse. <<<");break;
		case 102:sprintf(error,"%s",">>> Error 102: Invalid calibration type when configuring a AD input. <<<");break;
		case 103:sprintf(error,"%s",">>> Error 103: Invalid end advice type when configuring a AD input. <<<");break;
		case 104:sprintf(error,"%s",">>> Error 104: Invalid usage type when configuring a AD input. <<<");break;
		case 105:sprintf(error,"%s",">>> Error 105: Invalid Duty Cycle value when configuring a PWM outp. <<<");break;
		case 106:sprintf(error,"%s",">>> Error 106: Invalid Source Frequency value when configuring a PW. <<<");break;
		case 107:sprintf(error,"%s",">>> Error 107: Invalid Cycle type value when configuring a PWM outp. <<<");break;
		case 108:sprintf(error,"%s",">>> Error 108: Problems when opening the device driver. <<<");break;
		case 109:sprintf(error,"%s",">>> Error 109: Problems when closing the device driver. <<<");break;
		case 110:sprintf(error,"%s",">>> Error 110: Error when executing a device driver IOCTL command. <<<");break;
		case 111:sprintf(error,"%s",">>> Error 111: Trying to use an already used timer. <<<");break;
		case 112:sprintf(error,"%s",">>> Error 112: Trying to disconnect a not used timer. <<<");break;
		case 113:sprintf(error,"%s",">>> Error 113: There is no free timer available. <<<");break;
		case 114:sprintf(error,"%s",">>> Error 114: IOs Module already initialized. <<<");break;
		case 115:sprintf(error,"%s",">>> Error 115: IOs Module not yet initialized. <<<");break;
		case 116:sprintf(error,"%s",">>> Error 116: IOs Module already started. <<<");break;
		case 117:sprintf(error,"%s",">>> Error 117: IOs Module not yet started. <<<");break;
		case 118:sprintf(error,"%s",">>> Error 118: Error reading analog input. <<<");break;
		case 119:sprintf(error,"%s",">>> Error 119: iButton searching in progress. <<<");break;
		case 120:sprintf(error,"%s",">>> Error 120: 1-WIRE automatic update in progress. <<<");break;
		case 121:sprintf(error,"%s",">>> Error 121: More than MAX_OW_DEVICES in 1-WIRE. <<<");break;
		case 122:sprintf(error,"%s",">>> Error 122: iButton data size too big. <<<");break;
		case 123:sprintf(error,"%s",">>> Error 123: iButton error reading data. <<<");break;
		case 124:sprintf(error,"%s",">>> Error 124: PPS led is in use. <<<");break;
		/* GSM error */
		case 200:sprintf(error,"%s",">>> Error 200: GSM failure. <<<");break;
		case 201:sprintf(error,"%s",">>> Error 201: Connection failure with the net. <<<");break;
		case 202:sprintf(error,"%s",">>> Error 202: Me phone adaptor link reserved. <<<");break;
		case 203:sprintf(error,"%s",">>> Error 203: Operation not allowed by the GSM. <<<");break;
		case 204:sprintf(error,"%s",">>> Error 204: Operation not supported. <<<");break;
		case 205:sprintf(error,"%s",">>> Error 205: Phone SIM PIN required. Phone Code Locked. <<<");break;
		case 206:sprintf(error,"%s",">>> ERROR 206: SIM not inserted. <<<");break;
		case 207:sprintf(error,"%s",">>> Error 207: SIM PIN required. <<<");break;
		case 208:sprintf(error,"%s",">>> Error 208: SIM PUK required. <<<");break;
		case 209:sprintf(error,"%s",">>> Error 209: SIM failure. Contact operator. <<<");break;
		case 210:sprintf(error,"%s",">>> Error 210: SIM is busy. <<<");break;
		case 211:sprintf(error,"%s",">>> Error 211: SIM operation wrong. <<<");break;
		case 212:sprintf(error,"%s",">>> Error 212: SIM incorrect password, change. <<<");break;
		case 213:sprintf(error,"%s",">>> Error 213: SIM PIN2 required. <<<");break;
		case 214:sprintf(error,"%s",">>> Error 214: SIM PUK2 required. <<<");break;
		case 215:sprintf(error,"%s",">>> Error 215: GSM folder full( PhoneBook / SIM). <<<");break;
		case 216:sprintf(error,"%s",">>> Error 216: Index non used or major than max index(SMS/PB). <<<");break;
		case 217:sprintf(error,"%s",">>> . <<<");break;
		case 218:sprintf(error,"%s",">>> Error 218: There is a memory failure (SIM/ME). <<<");break;
		case 219:sprintf(error,"%s",">>> Error 219: Text string too long. <<<");break;
		case 220:sprintf(error,"%s",">>> Error 220: Invalid characters in text string. <<<");break;
		case 221:sprintf(error,"%s",">>> Error 221: The dial string is too long. <<<");break;
		case 222:sprintf(error,"%s",">>> Error 222: Dial String has invalid characters. <<<");break;
		case 223:sprintf(error,"%s",">>> Error 223: There is no GSM network service. <<<");break;
		case 224:sprintf(error,"%s",">>> Error 224: GSM Network returns timeout. <<<");break;
		case 225:sprintf(error,"%s",">>> Error 225: Unknown SMS error on the function. <<<");break;
		//SMS FAILURES
		case 226:sprintf(error,"%s",">>> Error 226: Failure on sending a SMS. <<<");break;
		case 227:sprintf(error,"%s",">>> Error 227: SMS Service of me reserved. <<<");break;
		case 228:sprintf(error,"%s",">>> Error 228: SMS Operation not allowed. <<<");break;
		case 229:sprintf(error,"%s",">>> Error 229: SMS Operation not supported. <<<");break;
		case 230:sprintf(error,"%s",">>> Error 230: Invalid PDU sending SMS. <<<");break;
		case 231:sprintf(error,"%s",">>> Error 231: SMSC Unknown. <<<");break;
		case 232:sprintf(error,"%s",">>> Error 232: CNMA ACK not received. <<<");break;
		case 233:sprintf(error,"%s",">>> Error 233: Unknown Error on SMSs. <<<");break;
		case 234:sprintf(error,"%s",">>> Error 234: Remote side is busy. <<<");break;
		case 235:sprintf(error,"%s",">>> Error 235: No answer at the peer side. <<<");break;
		case 236:sprintf(error,"%s",">>> Error 236: Network has released the current call. <<<");break;
		case 237:sprintf(error,"%s",">>> Error 237: Error if trying to dial when it is on conversation or other. <<<");break;
		case 238:sprintf(error,"%s",">>> Error 238: GSM Initialization Failure: Due to communication lack. <<<");break;
		case 239:sprintf(error,"%s",">>> Error 239: GSM Function securety timeout. <<<");break;
		case 240:sprintf(error,"%s",">>> Error 240: GSM is in data mode. <<<");break;
		case 241:sprintf(error,"%s",">>> Error 241: GSM communication lost. <<<");break;
		case 242:sprintf(error,"%s",">>> Error 242: GSM is being initialized. <<<");break;
		case 243:sprintf(error,"%s",">>> Error 243: Non-valid GPRS parameters. <<<");break;
		case 244:sprintf(error,"%s",">>> Error 244: GSM Module already initialized. <<<");break;
		case 245:sprintf(error,"%s",">>> Error 245: GSM Module not yet initialized. <<<");break;
		case 246:sprintf(error,"%s",">>> Error 246: GSM Module already started. <<<");break;
		case 247:sprintf(error,"%s",">>> Error 247: GSM Module not yet started. <<<");break;
		case 248:sprintf(error,"%s",">>> Error 248: Error due to Failure on Data. <<<");break;
		case 249:sprintf(error,"%s",">>> Error 249: Network Lock. <<<");break;
		case 250:sprintf(error,"%s",">>> Error 250: Unknown SMS format. <<<");break;
		case 251:sprintf(error,"%s",">>> Error 251: Error on the Audio file. <<<");break;
		case 252:sprintf(error,"%s",">>> Error 252: GSM HW has an error and does not switch on. <<<");break;
		case 253:sprintf(error,"%s",">>> Error 253: PLMN selection failure. <<<");break;
		case 254:sprintf(error,"%s",">>> Error 254: NO DIALTONE present. <<<");break;
		case 255:sprintf(error,"%s",">>> Error 255: Error opening digital audio system. <<<");break;
		case 256:sprintf(error,"%s",">>> Error 256: Error allocating HW structure. <<<");break;
		case 257:sprintf(error,"%s",">>> Error 257: Error initializing HW structure. <<<");break;
		case 258:sprintf(error,"%s",">>> Error 258: Error setting access type. <<<");break;
		case 259:sprintf(error,"%s",">>> Error 259: Error setting sample format. <<<");break;
		case 260:sprintf(error,"%s",">>> Error 260: Error setting sample rate. <<<");break;
		case 261:sprintf(error,"%s",">>> Error 261: Error setting channel count. <<<");break;
		case 262:sprintf(error,"%s",">>> Error 262: Error setting HW parameters. <<<");break;
		case 263:sprintf(error,"%s",">>> Error 263: Error in PCM prepare. <<<");break;
		case 264:sprintf(error,"%s",">>> Error 264: Error in PCM link. <<<");break;
		case 265:sprintf(error,"%s",">>> Error 265: Digital audio not allowed. <<<");break;
		case 266:sprintf(error,"%s",">>> Error 266: PS Busy. <<<");break;
		case 267:sprintf(error,"%s",">>> Error 267: SIM not ready. <<<");break;
		case 268:sprintf(error,"%s",">>> Error 268: Digital audio already opened. <<<");break;
		case 269:sprintf(error,"%s",">>> Error 269: Remote side rejects call (busy). <<<");break;
		case 270:sprintf(error,"%s",">>> Error 270: None connection time available. <<<");break;
		case 271:sprintf(error,"%s",">>> Error 271: No LCP time available. <<<");break;
		case 272:sprintf(error,"%s",">>> Error 272: No IP time available. <<<");break;
		case 273:sprintf(error,"%s",">>> Error 273: Not possible to create link. <<<");break;
		case 274:sprintf(error,"%s",">>> Error 274: Operation temporary not allow. <<<");break;
		case 275:sprintf(error,"%s",">>> Error 275: No GSM Coverage. <<<");break;
		case 276:sprintf(error,"%s",">>> Error 276: Unknown GSM model. <<<");break;
		case 277:sprintf(error,"%s",">>> Error 277: SIM Dual Mode disabled. Only PLS62-W. <<<");break;
		case 278:sprintf(error,"%s",">>> Error 278: GSM Modem not detected. <<<");break;
		case 279:sprintf(error,"%s",">>> Error 279: No resources available. <<<");break;
		case 280:sprintf(error,"%s",">>> Error 280: Generic unknown error. <<<");break;
		case 281:sprintf(error,"%s",">>> Error 281: Error 0 checking error response. <<<");break;
		case 282:sprintf(error,"%s",">>> Error 282: Error 1 checking error response. <<<");break;
		case 283:sprintf(error,"%s",">>> Error 283: Error 2 checking error response. <<<");break;
		case 284:sprintf(error,"%s",">>> Error 284: Error 3 checking error response. <<<");break;
		case 285:sprintf(error,"%s",">>> Error 285: Error 4 checking error response. <<<");break;
		case 286:sprintf(error,"%s",">>> Error 286: Unknown GSM model. <<<");break;
		case 287:sprintf(error,"%s",">>> Error 287: Error in SMS number. <<<");break;
		case 288:sprintf(error,"%s",">>> Error 288: Error in command format. <<<");break;
		case 289:sprintf(error,"%s",">>> Error 289: Error port mode changing speed. <<<");break;
		case 290:sprintf(error,"%s",">>> Error 290: Baud rate not supported. <<<");break;
		case 291:sprintf(error,"%s",">>> Error 291: Error setting baud rate. <<<");break;
		case 292:sprintf(error,"%s",">>> Error 292: Error in response handler. <<<");break;
		case 293:sprintf(error,"%s",">>> Error 293: Error SMS text mode. <<<");break;
		case 294:sprintf(error,"%s",">>> Error 294: Error reading PDU SMS. <<<");break;
		case 295:sprintf(error,"%s",">>> Error 295: Error getting GSM manufacturer. <<<");break;
		case 296:sprintf(error,"%s",">>> Error 296: GSM TX channel is not available. <<<");break;
		case 297:sprintf(error,"%s",">>> Error 297: Error create GSM socket. <<<");break;
		case 298:sprintf(error,"%s",">>> Error 208: Error in audio service. <<<");break;
		case 299:sprintf(error,"%s",">>> Error 299: Signal to get is not defined. <<<");break;
		case 300:sprintf(error,"%s",">>> Error 300: Signal to set is not defined. <<<");break;
		case 301:sprintf(error,"%s",">>> Error 301: Error setting UART signal. <<<");break;
		/* GPS error */
		case 401:sprintf(error,"%s",">>> Error: 401  Invalid GPS receiver type passed in configuration. <<<");break;
		case 402:sprintf(error,"%s",">>> Error: 402  Invalid GPS baudrate passed in configuration. <<<");break;
		case 403:sprintf(error,"%s",">>> Error: 403  Invalid parity type passed in configuration. <<<");break;
		case 404:sprintf(error,"%s",">>> Error: 404  Invalid byte length passed in configuration. <<<");break;
		case 405:sprintf(error,"%s",">>> Error: 405  Invalid number of stop bits passed in configuration. <<<");break;
		case 406:sprintf(error,"%s",">>> Error: 406  Invalid GPS protocol type passed in configuration. <<<");break;
		case 407:sprintf(error,"%s",">>> Error: 407  GPS protocol type not yet implemented. <<<");break;
		case 408:sprintf(error,"%s",">>> Error: 408  Not GPS connected in this port. <<<");break;
		case 409:sprintf(error,"%s",">>> Error: 409  The GPS is not started. <<<");break;
		case 410:sprintf(error,"%s",">>> Error: 410  The message was not sent to the GPS. <<<");break;
		case 411:sprintf(error,"%s",">>> Error: 411  No data received from GPS. <<<");break;
		case 412:sprintf(error,"%s",">>> Error: 412  Message received from GPS with bad checksum. No valid message. <<<");break;
		case 413:sprintf(error,"%s",">>> Error: 413  GPS port not opened. <<<");break;
		case 414:sprintf(error,"%s",">>> Error: 414  Switch on of the GPS not possible. <<<");break;
		case 415:sprintf(error,"%s",">>> Error: 415  Unlink from port was not possible. <<<");break;
		case 416:sprintf(error,"%s",">>> Error: 416  Unload external library was not possible. <<<");break;
		case 417:sprintf(error,"%s",">>> Error: 417  The request of the message is not allowed. <<<");break;
		case 418:sprintf(error,"%s",">>> Error: 418  The GPS has been already initialized calling to GPS_Initialize. <<<");break;
		case 419:sprintf(error,"%s",">>> Error: 419  The GPS is not initialized. Call GPS_Initialize before calling to GPS_Start. <<<");break;
		case 420:sprintf(error,"%s",">>> Error: 420  The GPS has been started. <<<");break;
		case 421:sprintf(error,"%s",">>> Error: 421  Not used. <<<");break;
		case 422:sprintf(error,"%s",">>> Error: 422  Not used. <<<");break;
		case 423:sprintf(error,"%s",">>> Error: 423  Not used. <<<");break;
		case 424:sprintf(error,"%s",">>> Error: 424  Function not allowed */. <<<");break;
		case 425:sprintf(error,"%s",">>> Error: 425  There are no memory resources left for the GPS to work properly */. <<<");break;
		case 426:sprintf(error,"%s",">>> Error: 426  The function is not supported in the GPS module */. <<<");break;
		case 427:sprintf(error,"%s",">>> Error: 427  Error when sending configuration message to the GPS module */. <<<");break;
		case 428:sprintf(error,"%s",">>> Error: 428  Error waiting the answer to a configuration message */. <<<");break;
		case 429:sprintf(error,"%s",">>> Error: 429  NACK received from GPS module */. <<<");break;
		case 430:sprintf(error,"%s",">>> Error: 430  the message with GPS information (position, time, speed, etc) has not been refreshed since last request. <<<");break;
		case 431:sprintf(error,"%s",">>> Error: 431  timeout when waiting for answer */. <<<");break;
		case 432:sprintf(error,"%s",">>> Error: 432  checksum error */. <<<");break;
		case 433:sprintf(error,"%s",">>> Error: 433  no valid measurement rate returned */. <<<");break;
		/* INET error */
		case 600:sprintf(error,"%s",">>> ERROR: 600  Inet already running. <<<");break;
		case 601:sprintf(error,"%s",">>> ERROR: 601  Inet not initialized. <<<");break;
		case 602:sprintf(error,"%s",">>> ERROR: 602  Inet not started. <<<");break;
		case 603:sprintf(error,"%s",">>> Error: 603  IF error by the kernel on IPget. <<<");break;
		case 604:sprintf(error,"%s",">>> Error: 604  No IP availble. Lack of Connection. <<<");break;
		case 605:sprintf(error,"%s",">>> Error: 605  GSM on Voice, inet stop not available. <<<");break;
		case 606:sprintf(error,"%s",">>> Error: 606  GSM on Call, inet stop not available. <<<");break;
		/* FMS error */
		case 1501:sprintf(error,"%s",">>> Error:1501	The FMS library has been already initialized and it is running. <<<");break;
		case 1502:sprintf(error,"%s",">>> Error:1502	Error when starting FMS as it has not been yet initialized. <<<");break;
		case 1503:sprintf(error,"%s",">>> Error:1503	Error when starting FMS as it is already started. <<<");break;
		case 1504:sprintf(error,"%s",">>> Error:1504	Error starting FMS library. <<<");break;
		case 1505:sprintf(error,"%s",">>> Error:1505	Error when creating the CAN socket. <<<");break;
		case 1506:sprintf(error,"%s",">>> Error:1506	Error when configuring the CAN socket. <<<");break;
		case 1507:sprintf(error,"%s",">>> Error:1507	Error when trying to bind to the CAN socket. <<<");break;
		case 1508:sprintf(error,"%s",">>> Error:1508	Error when creating a thread on the system. <<<");break;
		case 1509:sprintf(error,"%s",">>> Error:1509	Error when trying to finalize FMS without starting it first. <<<");break;
		case 1510:sprintf(error,"%s",">>> Error:1510	Error when loading FMS library. <<<");break;
		case 1511:sprintf(error,"%s",">>> Error:1511	Error when unloading functions from the system. <<<");break;
		case 1512:sprintf(error,"%s",">>> Error:1512	Error when loading functions on the system. <<<");break;
		case 1513:sprintf(error,"%s",">>> Error:1513	FMS library not validated as option in the unit. <<<");break;
		case 1514:sprintf(error,"%s",">>> Error:1514	Error when enabling the CAN driver. <<<");break;
		case 1515:sprintf(error,"%s",">>> Error:1515	FMS library option validation check up error. <<<");break;
		case 1516:sprintf(error,"%s",">>> Error:1516	Number of tire above the maximum value of 20. <<<");break;
		case 1517:sprintf(error,"%s",">>> Error:1517	Wrong FMS configuration. <<<");break;

		default:sprintf(error,">>> Error:%d	Unkown ERROR type. <<<", code);break;
	}
	return error;	
}

