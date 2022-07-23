
#define __GSM_C

#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>

#include "tbox/Common.h"
#include "tbox/GSM.h"

#include "Logger/Logger.h"

static gsmEvents_s   GsmEventBuffer[MAX_EVENTS];
static int           GsmEventsWr = 0;
static int           GsmEventsRd = 0;
static sem_t         GsmEventsSem;
static pthread_t gsmEvents;
int gsmError = FALSE;
int runGSMHandlerEvents = FALSE;

int gsmStart(void)
{
	int ReturnCode = NO_ERROR, iIsActive = 0;
	TGSM_MODULE_CONFIGURATION  gsmConfig;

	if(runGSMHandlerEvents == TRUE) {
		return -1;
	}

	// Obtaining the GSM Signal
	gsmConfig.gsm_action = gsm_event_handler;
	InitGsmEventBuffer();
   
	// Starting GSM
	memset( &gsmConfig, 0, sizeof( TGSM_MODULE_CONFIGURATION));

	// PIN Introduction
	// User: Introduces PIN Code <PIN_Code>
	strcpy( ( char*) gsmConfig.wCode, "1234");
  
	//User: Introduces ME Coder
	// ME Introduction <ME_Code>
	strcpy( ( char*) gsmConfig.wMECode, "");
	
	// GSM Initialize
	if(NO_ERROR != (ReturnCode =  GSM_Initialize((void*)(&gsmConfig))) )
	{
		if(GSM_ERR_ALREADY_INITIALIZED != ReturnCode)	//Error 244: GSM Module already initialized.
		{
			XLOG_DEBUG("{}",Str_Error(ReturnCode));
			return ReturnCode;
		}		
	}

	// GSM Start
	if (NO_ERROR != (ReturnCode = GSM_Start()) )
	{
		if(GSM_ERR_ALREADY_STARTED != ReturnCode)	//Error 246: GSM Module already started.
		{
			XLOG_DEBUG("{}",Str_Error(ReturnCode));
			return ReturnCode;
		}	
	}

	GSM_IsActive(&iIsActive);
    if (iIsActive) {
        // GSM_Set_Led_Mode(0);
		DIGIO_Set_LED_SW0_Input();
    };
	XLOG_DEBUG("GSM Moudle is active {}.", iIsActive ? "OK" : "FAIL");
	// Starting GSM Events Attention routine.
	runGSMHandlerEvents = TRUE;
	pthread_create(&gsmEvents, NULL, GSMHandleEvents, NULL);
	pthread_setname_np(gsmEvents, "GSM Handle");
	return 0;
}

void gsmStop(void)
{
	if(runGSMHandlerEvents == TRUE)
	{
		GSM_Finalize ( );
		// GSM_Set_Led_Mode(1);
		DIGIO_Set_LED_SW0_Output();
		// printf("TBox--> Ending the GSM module\n");
		XLOG_INFO("Ending the GSM module.");
		runGSMHandlerEvents = FALSE;
		sem_post(&GsmEventsSem);
		pthread_join(gsmEvents, NULL);
	}
}

static void gsm_event_handler( gsmEvents_s *pToEvent)
{
	int auxi = (GsmEventsWr + 1);

	// printf("gsm_event_handler CALLED\n");
	XLOG_DEBUG("gsm_event_handler CALLED.");
	GsmEventBuffer[GsmEventsWr] = *pToEvent;
	if( GsmEventsWr == GsmEventsRd) {
		GsmEventsWr = auxi;
	} else {
		if( auxi >= MAX_EVENTS) {
			auxi = 0;
		}
		if( auxi != GsmEventsRd) {
			GsmEventsWr = auxi;
		}
	}
	if( GsmEventsWr >= MAX_EVENTS) {
		GsmEventsWr = 0;
	}
	sem_post( &GsmEventsSem);
}

static int InitGsmEventBuffer( void)
{
	int retVal = NO_ERROR;

	memset( &GsmEventBuffer, 0x00, sizeof(GsmEventBuffer));
	GsmEventsWr = 0;
	GsmEventsRd = 0;
	sem_init( &GsmEventsSem, 0, 0);
	return retVal;
}

//-----------------------------------------------------------------//
// Function: void* GSMHandleEvents( void *arg)
// Input Params:
//    Arguments
// Output Params:
//    -
// Description:
//    This thread handles events given by the GSM. 
//-----------------------------------------------------------------//
void* GSMHandleEvents( void *arg)
{
    gsmEvents_s *owEvents;
	gsmEvents_s LocalEvent;

	while( runGSMHandlerEvents == TRUE){
		sem_wait( &GsmEventsSem);
		if( GsmEventsRd == GsmEventsWr) {
			continue;
		}
		LocalEvent = GsmEventBuffer[GsmEventsRd++];
		owEvents = &LocalEvent;
		if( GsmEventsRd >= MAX_EVENTS) {
			GsmEventsRd = 0;
		}
		switch ( owEvents->gsmEventType){
			case GSM_NO_SIGNAL:
				XLOG_WARN("GSM NO SIGNAL.");
				break;
 			case GSM_FAILURE:
				printf( "----------- GSM FAILURE ------------");
				XLOG_WARN("GSM FAILURE.");
				gsmError = TRUE;
				break;
			case GSM_COVERAGE:
				printf( "GSM EVENT--> GSM COVERAGE INFO: %d\n", owEvents->evBuffer[0]);
				XLOG_WARN("GSM EVENT--> GSM COVERAGE INFO: {:d}", owEvents->evBuffer[0]);
				break;
			case GSM_HIGHER_TEMP:
				printf( "GSM EVENT--> GSM HIGHER TEMP. Recommended GSM switch off\n");
				XLOG_WARN("GSM EVENT--> GSM HIGHER TEMP. Recommended GSM switch off.");
				// 蜂鸣器
				break;
			case GSM_GPRS_COVERAGE:
				printf( "GSM EVENT--> GPRS COVERAGE INFO: %d\n", owEvents->evBuffer[0]);
				XLOG_WARN("GSM EVENT--> GPRS COVERAGE INFO: {:d}", owEvents->evBuffer[0]);
				break;
			case GSM_BOARD_TEMP:
				printf( "GSM EVENT--> BOARD TEMPERATURE WARNING!!\n");
				switch ( owEvents->evBuffer[0]){
					case 0:
						printf( "Below lowest temperature limit (immediate GSM switch-off)\n");
						XLOG_WARN("Below lowest temperature limit (immediate GSM switch-off).");
						// 蜂鸣器
						break;
					case 1:
						printf( "Below low temperature alert limit\n");
						XLOG_WARN("Below lowest temperature limit (immediate GSM switch-off).");
						// 蜂鸣器
						break;
					case 2:
						printf( "Normal operating temperature\n");
						break;
					case 3:
						printf( "Above upper temperature alert limit\n");
						XLOG_WARN("Above upper temperature alert limit.");
						// 蜂鸣器
						break;
					case 4:
						printf( "Above uppermost temperature limit (immediate GSM switch-off)\n");
						XLOG_WARN("Above uppermost temperature limit (immediate GSM switch-off).");
						// 蜂鸣器
						break;
					default:
						printf( "Unknown temperature status\n");
						XLOG_WARN("Unknown temperature status.");
						break;
				}
				break;
			default:
				printf( "GSM Signal Event not found ...%d \n", owEvents->gsmEventType);
				XLOG_WARN( "GSM Signal Event not found ...{:d} \n", owEvents->gsmEventType);
		}
	}
	return NULL;
}
