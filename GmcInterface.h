/*
 *  GmcInterface.h
 *  VOPM
 *
 *  Created by osoumen on 2013/09/08.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */

#pragma once

#define CGIMIC_ERR_NONE						0
#define CGIMIC_ERR_UNKNOWN					-1
#define CGIMIC_ERR_INVALID_PARAM			-2
#define CGIMIC_ERR_UNSUPPORTED				-3
#define CGIMIC_ERR_NODEVICE					-1000
#define CGIMIC_ERR_NOT_IMPLEMENTED			-9999

enum ChipType {
	CHIP_UNKNOWN = 0,
	CHIP_OPNA,
	CHIP_OPM,
	CHIP_OPN3L,
};

class GimicParam
{
public:
	GimicParam() : ssgVol(0), clock(0) {
	};
	
	unsigned char	ssgVol;
	int				clock;
};

struct Devinfo{
	char Devname[16];
	char Rev;
	char Serial[15];
};


class GmcInterface {
public:
	struct MSG{
		unsigned char	len;
		unsigned char	dat[8];	// 最大メッセージ長は今のところ7byte.
		int				wait;	// 非同期メッセージの発動までのサイクル数（いまのところ１サイクル=1ms）
	};

	GmcInterface() : mIsInitialized(false), chiptype(CHIP_UNKNOWN) {}
	virtual ~GmcInterface() {}
	
public:
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
	virtual void Out(unsigned int addr, unsigned char data, int msec_time) = 0;
//	virtual unsigned char In( int addr ) = 0;
	virtual void FrameTask() {}
	
private:
	virtual int sendMsg( MSG *data ) { return CGIMIC_ERR_NODEVICE; };
	virtual int transaction( MSG *txdata, unsigned char *rxdata, int rxsz ) { return CGIMIC_ERR_NODEVICE; };
	
protected:
	bool		mIsInitialized;
	ChipType	chiptype;
	GimicParam	gimicParam;
};
