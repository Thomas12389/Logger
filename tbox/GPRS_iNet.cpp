//*********************************************************************
//********************** File information *****************************
//**
//** Description: Contains the GSM iNet functions and commands
//**
//** Filename:      GPRS_iNET.c
//** Creation date: 05/07/2010
//**
//*********************************************************************
//******************** Revision history *******************************
//** Revision date       Comments                           Responsible
//** -------- ---------- ---------------------------------- -----------
//** P1A      05/07/2010 First release                      Owasys
//*********************************************************************

//-----------------------------------------------------------------//
// Function: GPRSiNetUsage()
// Input Params:
//    -
// Output Params:
//    -
// Description:
//    This function shows the GSM/GPRS commands for iNet demonstra-
//    tion
//-----------------------------------------------------------------//

#define __GPRS_INET_C

#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>

#include "tbox/Common.h"
#include "tbox/GPRS_iNet.h"

#include "Logger/Logger.h"

static int           InitInetEventBuffer( void);
static INET_Events   InetEventBuffer[MAX_EVENTS];
static int           InetEventsWr = 0;
static int           InetEventsRd = 0;
int	runiNetHandleEvents	= FALSE;
int	iNetFinalized	= TRUE;
pthread_t	iNetEvents;
sem_t	iNetHandlerSem;


void iNetStart(const char *userName, const char *passwd, const char *APN)
{
	TINET_MODULE_CONFIGURATION 	iNetConfiguration;
	GPRS_ENHANCED_CONFIGURATION  gprsConfiguration;
	int ReturnCode = NO_ERROR;

	sem_init( &iNetHandlerSem, 0, 0);
	runiNetHandleEvents = TRUE;
	pthread_create( &iNetEvents, NULL, iNetHandleEvents, NULL);
	pthread_setname_np(iNetEvents, "iNet Handle");
	pthread_detach( iNetEvents);

	strcpy((char *)gprsConfiguration.gprsUser,"test");
	strcpy((char *)gprsConfiguration.gprsPass,"test");
	strcpy((char *)gprsConfiguration.gprsDNS1,"8.8.8.8");
	strcpy((char *)gprsConfiguration.gprsDNS2,"");
	strcpy((char *)gprsConfiguration.gprsAPN,"3gnet");

	memset( &iNetConfiguration, 0, sizeof( TINET_MODULE_CONFIGURATION));
	iNetConfiguration.wBearer = INET_BEARER_ENHANCED_GPRS;
	iNetConfiguration.inet_action = inet_event_handler;
	InitInetEventBuffer();
	iNetConfiguration.wBearerParameters = (void*) &gprsConfiguration;
	
	iNet_Initialize ( ( void*) &iNetConfiguration);
	ReturnCode = iNet_Start ( );
	if( ReturnCode != NO_ERROR){
		iNetFinalized       = TRUE;
		runiNetHandleEvents = FALSE;
		sem_post    ( &iNetHandlerSem);
		sem_destroy ( &iNetHandlerSem);
		// printf("TBox--> ERROR Initializing the Internet session\n");
		XLOG_ERROR("{}",Str_Error(ReturnCode));
	} else {
		iNetFinalized       = FALSE;
		XLOG_INFO("Internet session started successfully.");
	}
}

void iNetStop( )
{
   runiNetHandleEvents = FALSE;
   sem_post( &iNetHandlerSem);
}

static void inet_event_handler( INET_Events *pToEvent)
{
	int auxi = (InetEventsWr+1);

	XLOG_DEBUG("inet_event_handler CALLED.");
	XLOG_DEBUG("pToEvent->evType = {}, pToEvent->evHandled = {}", pToEvent->evType, pToEvent->evHandled);
	InetEventBuffer[InetEventsWr] = *pToEvent;
	if( InetEventsWr == InetEventsRd) {
		InetEventsWr = auxi;
	} else {
		if( auxi >= MAX_EVENTS) {
			auxi = 0;
		}
		if( auxi != InetEventsRd) {
			InetEventsWr = auxi;
		}
	}
	if( InetEventsWr >= MAX_EVENTS) {
		InetEventsWr = 0;
	}
	sem_post( &iNetHandlerSem);
}

static int InitInetEventBuffer( void)
{
   int retVal = NO_ERROR;

   memset( &InetEventBuffer, 0x00, sizeof(InetEventBuffer));
   InetEventsWr = 0;
   InetEventsRd = 0;
   return retVal;
}

void* iNetHandleEvents( void *arg)
{
	int iNetIsActive = 0;
	INET_Events *iNet_Events;
	INET_Events LocalEvent;
	int retVal = NO_ERROR;

	while( runiNetHandleEvents == TRUE){
		sem_wait( &iNetHandlerSem);
		if( runiNetHandleEvents == FALSE) {
			break;
		}
		if( InetEventsRd == InetEventsWr) {
			continue;
		}
		
		LocalEvent = InetEventBuffer[InetEventsRd++];
		iNet_Events = &LocalEvent;
		if( InetEventsRd >= MAX_EVENTS) {
			InetEventsRd = 0;
		}
		switch ( iNet_Events->evType)
		{
		case INET_RELEASED:
            XLOG_TRACE( "TBox--> INET RELEASED.");
            runiNetHandleEvents = FALSE;
			sem_post(&simSem);		//
			// printf("xxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
            break;
        default:
            XLOG_TRACE( "TBox--> Signal Event not found ...{:d}", iNet_Events->evType);
		}

	}

	if( runiNetHandleEvents == FALSE){
		if( iNetFinalized == FALSE){
			XLOG_TRACE( "TBox--> iNet Finalizing....");
			iNet_IsActive ( &iNetIsActive);
			if( iNetIsActive == TRUE){
				// printf("iNetHandleEvents **************************\n");
				retVal = iNet_Finalize();
				// printf("iNetHandleEvents **************************\n");
				if( (retVal == ERROR_INET_GSM_ON_VOICE) || (retVal == ERROR_INET_GSM_ON_CALL)) {
					printf("Error ending INET, still up!!\n");
					XLOG_ERROR("{}",Str_Error(retVal));
					runiNetHandleEvents = TRUE;
				} else {
					XLOG_INFO("iNet Moudle Finalized.");
					sem_destroy( &iNetHandlerSem);
					iNetFinalized = TRUE;
				}
			}
		}
	}
	
	XLOG_TRACE( "TBox--> Leaving iNetHandleEvents Thread.");
	pthread_exit( NULL);
}

void iNet_DataCounters( )
{
	int   retVal;
	long  TxBytes, RxBytes;

	retVal = iNet_GetDataCounters ( &TxBytes, &RxBytes);
	if( retVal != NO_ERROR){
		printf("TBox--> ERROR Getting Counters.\n");
		printf("%s\n",Str_Error(retVal));
	} else {
		printf("TBox--> OK Data Counters: \n\t-->Tx:%ld\n\n\t-->Rx:%ld\n", TxBytes, RxBytes);
	}
	return;
}

void iNet_Active( )
{
	int   iActive;

	iNet_IsActive ( &iActive);
	if( iActive == 1){
		printf("TBox--> iNet is active\n");
	} else {
		printf("TBox--> iNet is NOT active\n");
	}
	return;
}
