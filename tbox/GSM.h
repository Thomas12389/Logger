//*********************************************************************
//*************  Copyright (C) 2010        OWASYS  ********************
//*********************************************************************
//**
//** The copyright to the computer programs here in is the property of
//** OWASYS The programs may be used and/or copied only with the
//** written permission from OWASYS or in accordance with the terms and
//** conditions stipulated in the agreement/contract under which the
//** programs have been supplied.
//**
//*********************************************************************
//********************** File information *****************************
//**
//** Description:
//**
//** Filename:      Test_GSM.h
//** Creation date: 01/07/2010
//**
//*********************************************************************
//******************** Revision history *******************************
//** Revision date       Comments                           Responsible
//** -------- ---------- ---------------------------------- -----------
//** P1A      01/07/2010 First release                      Owasys
//*********************************************************************

#ifndef __GSM_H
   #define __GSM_H

   //-----------------------------------------------------------------//
   //System Includes
	
	//#include "owa4x/INET_ModuleDefs.h"
   //-----------------------------------------------------------------//
   //User Includes
   //-----------------------------------------------------------------//
//   #include "owa4x/GSM_ModuleDefs.h"
   //-----------------------------------------------------------------//
   //Defines
   //-----------------------------------------------------------------//

   //-----------------------------------------------------------------//
   //Macros
   //-----------------------------------------------------------------//
   #define MAX_EVENTS     10
   //-----------------------------------------------------------------//
   //Public Functions prototypes
   //-----------------------------------------------------------------//
	void *GSMHandleEvents(void *arg);
	int gsmStart(void);
	void gsmStop(void);
   //User Functions
	#ifdef __GSM_C

		//-----------------------------------------------------------------//
		//Internal Functions prototypes
		//-----------------------------------------------------------------//
		static void gsm_event_handler(gsmEvents_s *pToEvent);   
		static int  InitGsmEventBuffer(void);
		//-----------------------------------------------------------------//

	#endif

#endif

