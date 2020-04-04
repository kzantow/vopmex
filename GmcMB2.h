/*
 *  GmcMB2.h
 *  VOPM
 *
 *  Created by osoumen 2013/09/08.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */
#pragma once

#include "GmcInterface.h"
#include "OPMdrv.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <vector>
#include <time.h>
#include "ringbuff.h"
#include "ControlUSB.h"


class GmcMB2 : public GmcInterface, ControlUSB {
public:
	GmcMB2(OPMDRV *drv);
	~GmcMB2();
	
public:
	virtual int		Init(void);
	virtual int		AvailableChips();
	
	virtual void	Out(unsigned int addr, unsigned char data, int msec_time);
	virtual void	FrameTask();
	
	virtual	int		Reset(void);
	
	int		SetPLLClock(int clock);
private:
	GmcMB2();
	
	OPMDRV				*mDrv;
	
	CRingBuff<GmcInterface::MSG>		mRbuff;
	volatile int				*mReg;
	int							mPllClock;
	
	//ƒXƒŒƒbƒh
	void Tick(void);
#ifdef _WIN32
	static DWORD WINAPI		commThread(LPVOID arg);
	HANDLE				mCommThread;
	CRITICAL_SECTION	csection;
	HANDLE				mWaitObj;
	volatile long		mIsThRun;
#else
	static void *commThread( void *arg );
	pthread_t			mCommThread;
	pthread_mutex_t		mMutex;
	pthread_cond_t		mCond;
	volatile int		mIsThRun;
#endif
	virtual	int sendMsg( MSG *data );
	virtual	int transaction( MSG *txdata, unsigned char *rxdata, int rxsz );
	
	virtual void		onDeviceAdded();
	virtual void		onDeviceRemoved();

};
