/*
 *  Gmcc86.h
 *  VOPM
 *
 *  Created by osoumen 2013/09/08.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */
#pragma once

#include "GmcInterface.h"
#include "c86ctl.h"

class Gmcc86 : public GmcInterface {
public:
	Gmcc86();
	~Gmcc86(void);

	virtual int Init(void);
	virtual int AvailableChips();
	virtual int SetSSGVolume(unsigned char vol);
	virtual int GetSSGVolume(unsigned char *vol);
	virtual int SetPLLClock(unsigned int clock);
	virtual int GetPLLClock(unsigned int *clock);
	virtual int GetMBInfo(struct Devinfo *info);
	virtual int GetModuleInfo(struct Devinfo *info);
	virtual int GetFWVer(unsigned int *major, unsigned int *minor, unsigned int *rev, unsigned int *build);
	virtual int Reset(void);
	virtual int GetChipStatus(unsigned int addr, unsigned char *status);
	virtual void Out(unsigned int addr, unsigned char data, int msec_time);

private:
	c86ctl::IRealChip2		*pRC;
	c86ctl::IGimic2			*pGimic;

	int						*mReg;
};
