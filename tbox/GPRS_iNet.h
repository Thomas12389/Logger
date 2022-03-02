//*********************************************************************
//********************** File information *****************************
//**
//** Description: This is the header file of GPRS_iNet.c
//**
//** Filename:      GPRS_iNet.h
//** Creation date: 05/07/2010
//**
//*********************************************************************
//******************** Revision history *******************************
//** Revision date       Comments                           Responsible
//** -------- ---------- ---------------------------------- -----------
//** P1A      05/07/2010 First release                      Owasys
//*********************************************************************

#ifndef __GSM_GPRSINET_H
	#define __GSM_GPRSINET_H
	
	//-----------------------------------------------------------------//
	//System Includes
	//-----------------------------------------------------------------//
	#include <signal.h>
	#include <semaphore.h>
	//-----------------------------------------------------------------//
	//User Includes
	//-----------------------------------------------------------------//
	#include "owa4x/GSM_ModuleDefs.h"
	#include "GSM.h"
	//-----------------------------------------------------------------//
	//Defines
	//-----------------------------------------------------------------//
	#define MAX_DATA_SIZE   1024      //Max. packet size for Tx/Rx data to server
	//-----------------------------------------------------------------//
	//Macros
	//-----------------------------------------------------------------//
	//-----------------------------------------------------------------//
	//Public Functions prototypes
	//-----------------------------------------------------------------//
	void iNetStart             (const char *userName, const char *passwd, const char *APN);
	void iNetStop              ( );
	void iNet_DataCounters     ( );
	void iNet_Active           ( );
	void *iNetHandleEvents  ( void *arg);
	
	#ifdef __GPRS_INET_C

		static void inet_event_handler( INET_Events *pToEvent);
		
	#endif
	
	extern sem_t	simSem;
	
#endif

