/*
 *  GmcMB2.cpp
 *  VOPM
 *
 *  Created by osoumen on 2013/09/08.
 *  Copyright 2013 __MyCompanyName__. All rights reserved.
 *
 */

#include "GmcMB2.h"

#if __APPLE_CC__
#include <Carbon/Carbon.h>
#endif


#define GIMIC_USBVID 0x16c0
#define GIMIC_USBPID 0x05e5


//�X���b�h
#ifdef _WIN32
#define ENTER_CSECTION	if (mIsThRun) EnterCriticalSection(&csection);
#define LEAVE_CSECTION	if (mIsThRun) LeaveCriticalSection(&csection);
#else
#define ENTER_CSECTION	if (mIsThRun) pthread_mutex_lock( &mMutex );
#define LEAVE_CSECTION	if (mIsThRun) pthread_mutex_unlock( &mMutex );
#endif

GmcMB2::GmcMB2(OPMDRV *drv)
: mReg(NULL)
, mPllClock(3580000)
, mCommThread(NULL)
#ifdef _WIN32
, mWaitObj(NULL)
#endif
, mIsThRun(0)
{
	mDrv = drv;
	mReg = new int[256];
}

GmcMB2::~GmcMB2()
{
	onDeviceRemoved();
	delete [] mReg;
}

void GmcMB2::onDeviceAdded()
{
#if _WIN32
	InitializeCriticalSection( &csection );
	mWaitObj = CreateEvent(NULL, FALSE, FALSE, NULL );
#else
	pthread_mutex_init( &mMutex, NULL );
	pthread_cond_init( &mCond, NULL );
#endif

	mRbuff.alloc(4096);
	
	Reset();
	SetPLLClock(mPllClock);
	
	//�X���b�h�̍쐬�A�J�n
	mIsThRun = 1;
#if _WIN32
	DWORD dwID;
	mCommThread = CreateThread(NULL, 0, commThread, this, 0, &dwID);
#else
	int	returnVal;
	returnVal = pthread_create(&mCommThread, NULL, &commThread, this);
	assert(!returnVal);
#endif
	mDrv->SetLocalOn(false);
}

void GmcMB2::onDeviceRemoved()
{
	if (mIsThRun > 0) { 
#if _WIN32
		::InterlockedDecrement(&mIsThRun);
		SetEvent(mWaitObj);
		WaitForSingleObject(mCommThread, INFINITE);
		DeleteCriticalSection( &csection );
		CloseHandle( mWaitObj );
#else
		OSAtomicDecrement32(&mIsThRun);
		pthread_cond_signal( &mCond );
		pthread_join(mCommThread, NULL);
		pthread_mutex_destroy( &mMutex );
		pthread_cond_destroy( &mCond );
#endif			
	}
	if (isPlugged()) {
		removeDevice();
	}
	mRbuff.freeres();
		
	mDrv->SetLocalOn(true);
}

int GmcMB2::Init(void)
{
	BeginPortWait(GIMIC_USBVID, GIMIC_USBPID);
	
	mIsInitialized = true;
	return CGIMIC_ERR_NONE;
}

int GmcMB2::SetPLLClock(int clock)
{
	mPllClock = clock;	// �ڑ����ɔ��f������
	if (isPlugged()) {
		return GmcInterface::SetPLLClock(clock);
	}
	return CGIMIC_ERR_NONE;
}

void GmcMB2::FrameTask()
{
	//	ENTER_CSECTION
	
	//������Tick�����̏I����҂��Ă��܂�����A�킴�킴�ʃX���b�h�ɂ����Ӗ����Ȃ�
	//Tick�����̍Œ���������V�O�i���͋�U��ɂȂ邪�A�����Ń����_�[�X���b�h�����b�N��������}�Y��
	//��U��ɂȂ����ꍇ�A���̃t���[���܂�Tick�������҂�����邪�A�ʏ��Tick������1�t���[���̎��ԓ��ɏI���͂��Ȃ̂ŁA
	//���̊֐����Ă΂��^�C�~���O���ނ��둁�܂��Ă���\��������A��������Ԃɖ߂�ƍl������B
	
#ifdef WIN32
	SetEvent(mWaitObj);
#else
	pthread_cond_signal( &mCond );
#endif
	
	//	LEAVE_CSECTION
}

int GmcMB2::AvailableChips()
{
	return isPlugged()?1:0;
}

#ifdef WIN32
DWORD WINAPI GmcMB2::commThread(LPVOID arg)
#else
void *GmcMB2::commThread( void *arg )
#endif
{
	GmcMB2	*This = static_cast<GmcMB2*>(arg);
	while (This->mIsThRun) {
		This->Tick();
	}
	return NULL;
}

void GmcMB2::Tick(void)
{
	ENTER_CSECTION

	/*
#ifdef WIN32
	WaitForSingleObject(mWaitObj, 1000);
#else
	timespec timeout;
	timeout.tv_sec = time(NULL) + 1;
	timeout.tv_nsec = 0;
	pthread_cond_timedwait(&mCond, &mMutex, &timeout);
#endif
	 */
 
	//	int	evtnum = sRbuff.get_length();
	//	int loopnum = evtnum;
	int tick = 0;
	
	if ( mRbuff.get_length() == 0 ) {
#ifndef WIN32
		usleep(1000);
#else
		Sleep(1);
#endif
	}
	else {
		while ( mRbuff.get_length() > 0 ) {
			static const int	PACKET_SIZE = 64;
			unsigned char	buf[PACKET_SIZE];
			int				bufpos	=0;
			int				size	=0;
			GmcMB2::MSG	data;
			
			while ( mRbuff.get_length() > 0 ) {
				if ( tick < mRbuff.query_read_ptr()->wait ) {
					break;
				}
				if ( size+mRbuff.query_read_ptr()->len > PACKET_SIZE ) {
					break;
				}
				if ( !mRbuff.read1(&data) ) {
					//�����Ɣz��Alloc����Ă���΂����ɂ͗��Ȃ��͂�
					break;
				}
				size += data.len;
				//printf("sz = %d\n",sz);
				for( int j=0; j<data.len; j++ ) {
					buf[bufpos++] = data.dat[j];
				}
				//evtnum--;
			}
			/*
			 for(int idx=0; idx<loopnum; idx++) {
			 int len		= mRbuff.idx_read_ptr(idx)->len;
			 int wait	= sRbuff.idx_read_ptr(idx)->wait;
			 
			 //�\�莞���ɗ��Ă��Ȃ��C�x���g�̓X���[
			 if( tick > wait ) {
			 continue;
			 }
			 //���ɏ������߂�ʂ𒴂�����Pms��ɉ�
			 if( (size+len) > 64 ) {
			 sRbuff.idx_read_ptr(idx)->wait++;
			 continue;
			 }
			 if( !sRbuff.read1(&data) ) {
			 //�����Ɣz��Alloc����Ă���΂����ɂ͗��Ȃ��͂�
			 break;
			 }
			 size += data.len;
			 //printf("sz = %d\n",sz);
			 for( int j=0; j<data.len; j++ ) {
			 buf[bufpos++] = data.dat[j];
			 }
			 evtnum--;
			 }
			 */
			//64�o�C�g���x�̏ꍇ��0xff�͕s�v�H
			if( size>0 && size<PACKET_SIZE ) {
				buf[size] = 0xff;
				size++;
			}
			
			if ( size > 0 ) {
				int len = size;
				bulkWriteAsync(buf, len);
			}
			else {
#ifndef WIN32
				//usleep(1000);
#endif
			}
#ifdef WIN32
			//Sleep(1);
#endif
			tick++;
		}
	}
	
	//�ǂݏo���ʒu�͍Ō�ɂ܂Ƃ߂Đi�߂�
	//���b�N�������_�ł̌����������������Ȃ��̂ŁA���C���X���b�h�ōs����ǉ����Ƀ��b�N���s�v
	/*
	 for ( int i=0; i<loopnum; i++ ) {
	 sRbuff.adv_ridx();
	 }
	 */
	LEAVE_CSECTION
}

int GmcMB2::Reset(void)
{
	int	ret = GmcInterface::Reset();
	
	//���W�X�^�ۑ��l�̏�����
	for ( int i=0; i<256; i++ ) {
		mReg[i] = -1;
	}
	
	return ret;
}

void GmcMB2::Out(unsigned int addr, unsigned char data, int msec_time)
{
	if ( !isPlugged() ) {
		return;
	}
	
	if (addr == 0x01 || addr == 0x08 || (addr >= 0x0f && addr <= 0x12) ||
		addr == 0x14 || (addr >= 0x18 && addr <= 0x19) || addr == 0x1b ||
		addr >= 0x20 ) {
		
		if ( mReg[addr] != data || addr == 0x01 || addr == 0x08 || addr == 0x14) {
			mReg[addr] = data;
			
			//printf("addr = %02x data = %02x\n", buf[0], buf[1]);
			//printf("usec_time = %d\n",usec_time);
			if ( addr >= 0xfc ) {
				addr -= 0xe0;
			}
			MSG d = { 2, { addr&0xff, data }, msec_time };
			mRbuff.write1(d);
			//int ret = sendMsg( &d );
			//Tick();
		}
	}
}

int GmcMB2::sendMsg( MSG *data )
{
	unsigned char buf[66];
	
	if ( !isPlugged() ) {
		return CGIMIC_ERR_NODEVICE;
	}
	
	buf[0] = 0;		// HID report ID
	int	size = data->len;
	if ( size > 0 ) {
		memcpy( buf, &data->dat[0], size );
		if ( size < 64 ) {
			memset( &buf[size], 0xff, 64-size );
		}
		
		int	len = size;
		ENTER_CSECTION
		bulkWrite(buf, len);
		LEAVE_CSECTION
	}
	
	return CGIMIC_ERR_NONE;
}

int GmcMB2::transaction( MSG *txdata, unsigned char *rxdata, int rxsz )
{
	unsigned char buf[66];
	
	if ( !isPlugged() ) {
		return CGIMIC_ERR_NODEVICE;
	}
	
	buf[0] = 0;		// HID report ID
	
	ENTER_CSECTION
	{
		int size = txdata->len;
		if ( size > 0 ) {
			memcpy( &buf[1], &txdata->dat[0], size );
			if( size < 64 ) {
				memset( &buf[size], 0xff, 64-size );
			}
			
			int	len = size;
			bulkWrite(buf, len);
		}
		
		
		int len = 0;
		while (len == 0) {
			len = bulkRead(buf, sizeof(buf));
			if (len == 0) {
				//printf("waiting...\n");
			}
			if (len < 0) {
				break;
				//printf("Unable to read()\n");
				return CGIMIC_ERR_UNKNOWN;
			}
#ifdef WIN32
			Sleep(2);
#else
			usleep(2*1000);
#endif
		}
		memcpy( rxdata, buf, rxsz );
	}
	LEAVE_CSECTION
	
	return CGIMIC_ERR_NONE;
}
