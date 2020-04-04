/*
 *  GmcInterface.cpp
 *  VOPM
 *
 *  Created by osoumen on 2013/09/14.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */

#include "GmcInterface.h"

int GmcInterface::Init(void)
{
	return false;
}

int GmcInterface::AvailableChips()
{
	return 0;
}

int GmcInterface::SetSSGVolume(unsigned char vol)
{
	//	if( chiptype != CHIP_OPNA )
	//		return CGIMIC_ERR_UNSUPPORTED;
	
	gimicParam.ssgVol = vol;
	MSG d = { 3, { 0xfd, 0x84, vol } };
	return sendMsg( &d );
}

int GmcInterface::GetSSGVolume(unsigned char *vol)
{
	//	if( chiptype != CHIP_OPNA )
	//		return CGIMIC_ERR_UNSUPPORTED;
	if( !vol )
		return CGIMIC_ERR_INVALID_PARAM;
	
	MSG d = { 2, { 0xfd, 0x86 } };
	int ret = transaction( &d, (unsigned char*)vol, 1 );
	
	gimicParam.ssgVol = *vol;
	return ret;
}

int GmcInterface::SetPLLClock(unsigned int clock)
{
	//TODO: OPNA OPM 以外のモジュールではエラーを返す
	
	//音源の動作クロックを変更
	//FD 83 WW XX YY ZZ : set PLL clock, WW=clock[7:0], XX=clock[15:8], YY=clock[23:16], ZZ=clock[31:24]
	gimicParam.clock = clock;
	MSG d = { 6, { 0xfd, 0x83, clock&0xff, (clock>>8)&0xff, (clock>>16)&0xff, (clock>>24)&0xff, 0 } };
	return sendMsg( &d );
}

int GmcInterface::GetPLLClock(unsigned int *clock)
{
	//	if( chiptype != CHIP_OPNA && chiptype != CHIP_OPM )
	//		return CGIMIC_ERR_UNSUPPORTED;
	
	if( !clock ) {
		return CGIMIC_ERR_INVALID_PARAM;
	}
	
	MSG d = { 2, { 0xfd, 0x85 } };
	int ret = transaction( &d, (unsigned char*)clock, 4 );
	
	gimicParam.clock = *clock;
	return ret;
}

int GmcInterface::GetMBInfo(struct Devinfo *info)
{
	int ret;
	
	if( !info ) {
		return CGIMIC_ERR_INVALID_PARAM;
	}
	
	MSG d = { 3, { 0xfd, 0x91, 0xff } };
	if( CGIMIC_ERR_NONE == (ret = transaction( &d, (unsigned char*)info, 32 )) ){
		char *p = &info->Devname[15];
		while( *p==0 || *p==-1 ) *p--=0;
		p = &info->Serial[14];
		while(*p==0||*p==-1) *p--=0;
	}
	return ret;
}

int GmcInterface::GetModuleInfo(struct Devinfo *info)
{
	int ret;
	
	if ( !info ) {
		return CGIMIC_ERR_INVALID_PARAM;
	}
	
	MSG d = { 3, { 0xfd, 0x91, 0 } };
	if( CGIMIC_ERR_NONE == (ret = transaction( &d, (unsigned char*)info, 32 )) ) {
		char *p = &info->Devname[15];
		while(*p==0||*p==-1) *p--=0;
		p = &info->Serial[14];
		while(*p==0||*p==-1) *p--=0;
	}
	return ret;
}

int GmcInterface::GetFWVer(unsigned int *major, unsigned int *minor, unsigned int *rev, unsigned int *build)
{
	unsigned char rx[16];
	MSG d = { 2, { 0xfd, 0x92 } };
	int ret;
	
	if( CGIMIC_ERR_NONE == (ret = transaction( &d, rx, 16 )) ){
		if( major ) *major = *((unsigned int *)&rx[0]);
		if( minor ) *minor = *((unsigned int *)&rx[4]);
		if( rev )   *rev   = *((unsigned int *)&rx[8]);
		if( build ) *build = *((unsigned int *)&rx[12]);
	}
	return ret;
}


int GmcInterface::Reset(void)
{
	// FD 82             : Software Reset
	int ret;
	// リセットコマンド送信
	MSG d = { 2, { 0xfd, 0x82, 0 } };
	ret =  sendMsg( &d );
	
	return ret;
}


int GmcInterface::GetChipStatus(unsigned int addr, unsigned char *status)
{
	if( !status ) {
		return CGIMIC_ERR_INVALID_PARAM;
	}
	
	unsigned char rx[4];
	MSG d = { 3, { 0xfd, 0x93, addr&0x01 } };
	int ret;
	
	if( CGIMIC_ERR_NONE == (ret = transaction( &d, rx, 4 )) ){
		*status = *((unsigned char*)&rx[0]);
	}
	return ret;
}
