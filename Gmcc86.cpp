/*
 *  GmcMB2.cpp
 *  VOPM
 *
 *  Created by osoumen on 2013/09/08.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */

#include <string.h>
#include "Gmcc86.h"

HMODULE hC86DLL = NULL;
c86ctl::IRealChipBase	*pChipBase = NULL;
int						sNumChips = 0;
bool					sUsedChips[4];

typedef HRESULT (WINAPI *TCreateInstance)( REFIID, LPVOID* );

Gmcc86::Gmcc86()
{
	mIsInitialized = false;
	pRC = NULL;
	pGimic = NULL;
	mReg = new int[256];
	//レジスタ保存値の初期化
	for ( int i=0; i<256; i++ ) {
		mReg[i] = -1;
	}
}

Gmcc86::~Gmcc86(void)
{
	if (pGimic) {
		pGimic->Release();
	}
	if (pRC) {
		pRC->Release();
	}
	delete [] mReg;
}

int Gmcc86::Init(void)
{
	// c86ctl.dll の読み込み
	if (hC86DLL == NULL) {
		hC86DLL = ::LoadLibrary("c86ctl.dll");
		if (hC86DLL == NULL) {
			return CGIMIC_ERR_NODEVICE;
		}
		// RealChipBaseの取得
		TCreateInstance pCI;
		pCI = (TCreateInstance)::GetProcAddress(hC86DLL, "CreateInstance");
		(*pCI)( c86ctl::IID_IRealChipBase, (void**)&pChipBase );
		pChipBase->initialize();
		sNumChips = pChipBase->getNumberOfChip();
		for (int i=0; i<4; i++) {
			sUsedChips[i] = false;
		}
	}

	// 使用されていないものを探す
	int	chipNum = -1;
	for (int i=0; i<sNumChips; i++) {
		if (sUsedChips[i] == false) {
			sUsedChips[i] = true;
			chipNum = i;
			break;
		}
	}
	if (chipNum == -1) {
		return CGIMIC_ERR_NODEVICE;
	}

	// IRealChip取得
	if (sNumChips > 0) {
		pChipBase->getChipInterface( chipNum, c86ctl::IID_IRealChip2, (void**)&pRC );
	}
	pRC->reset();

	// IGimic取得
	if (S_OK == pRC->QueryInterface( c86ctl::IID_IGimic2, (void**)&pGimic )) {
		mIsInitialized = true;
	}	
	return CGIMIC_ERR_NONE;
}

int Gmcc86::AvailableChips()
{
	return mIsInitialized?1:0;
}

int Gmcc86::SetSSGVolume(unsigned char vol)
{
	int	ret = CGIMIC_ERR_NODEVICE;
	if (pGimic) {
		pGimic->setSSGVolume(vol);
		ret = CGIMIC_ERR_NONE;
	}
	return ret;
}

int Gmcc86::GetSSGVolume(unsigned char *vol)
{
	int	ret = CGIMIC_ERR_NODEVICE;
	if (pGimic) {
		pGimic->getSSGVolume(vol);
		ret = CGIMIC_ERR_NONE;
	}
	return ret;
}

int Gmcc86::SetPLLClock(unsigned int clock)
{
	int	ret = CGIMIC_ERR_NODEVICE;
	if (pGimic) {
		pGimic->setPLLClock(clock);
		ret = CGIMIC_ERR_NONE;
	}
	return ret;
}

int Gmcc86::GetPLLClock(unsigned int *clock)
{
	int	ret = CGIMIC_ERR_NODEVICE;
	if (pGimic) {
		pGimic->getPLLClock(clock);
		ret = CGIMIC_ERR_NONE;
	}
	return ret;
}

int Gmcc86::GetMBInfo(struct Devinfo *info)
{
	int	ret = CGIMIC_ERR_NODEVICE;
	if (pGimic) {
		c86ctl::Devinfo	c86Info;
		pGimic->getMBInfo(&c86Info);
		memcpy(&info->Devname, &c86Info.Devname, 16);
		info->Rev = c86Info.Rev;
		memcpy(&info->Serial, &c86Info.Serial, 15);
		ret = CGIMIC_ERR_NONE;
	}
	return ret;
}

int Gmcc86::GetModuleInfo(struct Devinfo *info)
{
	int	ret = CGIMIC_ERR_NODEVICE;
	if (pGimic) {
		c86ctl::Devinfo	c86Info;
		pGimic->getModuleInfo(&c86Info);
		memcpy(&info->Devname, &c86Info.Devname, 16);
		info->Rev = c86Info.Rev;
		memcpy(&info->Serial, &c86Info.Serial, 15);
		ret = CGIMIC_ERR_NONE;
	}
	return ret;
}

int Gmcc86::GetFWVer(unsigned int *major, unsigned int *minor, unsigned int *rev, unsigned int *build)
{
	int	ret = CGIMIC_ERR_NODEVICE;
	if (pGimic) {
		pGimic->getFWVer(major, minor, rev, build);
		ret = CGIMIC_ERR_NONE;
	}
	return ret;
}

int Gmcc86::Reset(void)
{
	int	ret = CGIMIC_ERR_NODEVICE;
	if (pRC) {
		pRC->reset();
		ret = CGIMIC_ERR_NONE;
	}
	//レジスタ保存値の初期化
	for ( int i=0; i<256; i++ ) {
		mReg[i] = -1;
	}
	return ret;
}

int Gmcc86::GetChipStatus(unsigned int addr, unsigned char *status)
{
	int	ret = CGIMIC_ERR_NODEVICE;
	if (pRC) {
		pRC->getChipStatus(addr, status);
		ret = CGIMIC_ERR_NONE;
	}
	return ret;
}

void Gmcc86::Out(unsigned int addr, unsigned char data, int msec_time)
{
	if (pRC) {
		if (addr == 0x01 || addr == 0x08 || (addr >= 0x0f && addr <= 0x12) ||
			addr == 0x14 || (addr >= 0x18 && addr <= 0x19) || addr == 0x1b ||
			addr >= 0x20 ) {
			
			if ( mReg[addr] != data || addr == 0x01 || addr == 0x08 || addr == 0x14) {
				mReg[addr] = data;
				pRC->out(addr, data);
			}
		}
	}
}

