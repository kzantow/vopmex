/*
 *  S98File.cpp
 *  VOPM
 *
 *  Created by osoumen on 12/11/04.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#include "S98File.h"
#include <string.h>

#if __APPLE_CC__
#include <Carbon/Carbon.h>
#endif

#if _WIN32
#include <windows.h>
#endif

//-----------------------------------------------------------------------------
S98File::S98File(int allocSize)
: m_pData( NULL )
, mDataSize( allocSize )
, mDataUsed( 0 )
, mDataPos( 0 )
, mTimeNumerator( 1 )
, mTimeDenominator( 1000 )
{
	if ( allocSize > 0 ) {
		m_pData = new unsigned char[allocSize];
	}
	BeginDump(0);
}

//-----------------------------------------------------------------------------
S98File::~S98File()
{
	if ( m_pData != NULL ) {
		delete [] m_pData;
	}
}

//-----------------------------------------------------------------------------
bool S98File::SetPos( int pos )
{
	if ( mDataSize < pos ) {
		return false;
	}
	mDataPos = pos;
	if ( mDataPos > mDataUsed ) {
		mDataUsed = mDataPos;
	}
	return true;
}

//-----------------------------------------------------------------------------
bool S98File::SaveToFile( const char *path, int clock )
{
	//�w�b�_�[��t���ă_���v���e���t�@�C���ɏ����o��
	S98Header	header = {
		{ 'S','9','8' },
		'3',
		mTimeNumerator,
		mTimeDenominator,
		0,								// COMPRESSING The value is 0 always.
		sizeof(S98Header) + mDataUsed,	// FILE OFFSET TO TAG
		sizeof(S98Header),				// FILE OFFSET TO DUMP DATA
		sizeof(S98Header) + mLoopPoint,	// FILE OFFSET TO LOOP POINT DUMP DATA
		1,								// DEVICE COUNT
		5,								// DEVICE TYPE (OPM)
		clock,							// CLOCK(Hz)
		0,								// PAN
		0								// RESERVE
	};
	
	char	tag[] = "[S98]title=Title\nartist=Artist\ncopyright=(c)\n";
	
#if __APPLE_CC__
	CFURLRef	savefile = CFURLCreateFromFileSystemRepresentation(NULL, (UInt8*)path, strlen(path), false);
	
	CFWriteStreamRef	filestream = CFWriteStreamCreateWithFile(NULL,savefile);
	if (CFWriteStreamOpen(filestream)) {
		CFWriteStreamWrite(filestream, reinterpret_cast<UInt8*> (&header), sizeof(S98Header) );
		CFWriteStreamWrite(filestream, m_pData, mDataUsed );
		CFWriteStreamWrite(filestream, reinterpret_cast<UInt8*> (tag), sizeof(tag) );
		CFWriteStreamClose(filestream);
	}
	CFRelease(filestream);
	CFRelease(savefile);
	
	return true;
#else
	HANDLE	hFile;
	
	hFile = CreateFile( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile != INVALID_HANDLE_VALUE ) {
		DWORD	writeSize;
		WriteFile( hFile, &header, sizeof(S98Header), &writeSize, NULL );
		WriteFile( hFile, m_pData, mDataUsed, &writeSize, NULL );
		WriteFile( hFile, tag, sizeof(tag), &writeSize, NULL );
		CloseHandle( hFile );
	}
	return true;
#endif
}

//-----------------------------------------------------------------------------
void S98File::SetResolution( int numerator, int denominator )
{
	mTimeNumerator = numerator;
	mTimeDenominator = denominator;
}

//-----------------------------------------------------------------------------
void S98File::BeginDump( int time )
{
	mDumpBeginTime = time;
	mPrevTime = mDumpBeginTime;
	for ( int i=0; i<256; i++ ) {
		mReg[i] = -1;
	}
	mDataUsed = 0;
	mDataPos = 0;
	mLoopPoint = 0;
	mIsEnded = false;
	
//	printf("--BeginDump--\n");
}

//-----------------------------------------------------------------------------
bool S98File::DumpReg( int addr, unsigned char data, int time )
{
	if (addr == 0x01 || addr == 0x08 || (addr >= 0x0f && addr <= 0x12) ||
		addr == 0x14 || (addr >= 0x18 && addr <= 0x19) || addr == 0x1b ||
		addr >= 0x20 ) {
		
		if ( mReg[addr] != data || addr == 0x01 || addr == 0x08 || addr == 0x14) {
			mReg[addr] = data;

			writeWaitFromPrev(time);
			
			if ( GetWritableSize() >= 3 ) {
				writeByte( 0x00 );	// DEVICE1(normal)
				writeByte( addr );
				writeByte( data );
			}
			
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
void S98File::MarkLoopPoint()
{
	mLoopPoint = mDataPos;
	//���[�v����͏�Ƀ��W�X�^���������܂��悤�ɂ���
	for ( int i=0; i<256; i++ ) {
		mReg[i] = -1;
	}
//	printf("--MarkLoopPoint--\n");
}

//-----------------------------------------------------------------------------
void S98File::EndDump(int time)
{
	if ( mDataUsed > 0 && mIsEnded == false ) {
		writeWaitFromPrev(time);
		writeEndByte();
		mIsEnded = true;
		
//		printf("--EndDump-- %d\n",time);
	}
}

//-----------------------------------------------------------------------------
bool S98File::writeByte( unsigned char byte )
{
	if ( ( mDataPos + 1 ) > (mDataSize-1) ) {	//END/LOOP���������߂�l�ɂP�o�C�g�c���Ă���
		return false;
	}
	m_pData[mDataPos] = byte;
	mDataPos++;
	if ( mDataPos > mDataUsed ) {
		mDataUsed = mDataPos;
	}
	return true;
}

//-----------------------------------------------------------------------------
bool S98File::writeEndByte()
{
	if ( ( mDataPos + 1 ) > mDataSize) {
		return false;
	}
	m_pData[mDataPos] = 0xfd;
	mDataPos++;
	if ( mDataPos > mDataUsed ) {
		mDataUsed = mDataPos;
	}
	return true;
}

//-----------------------------------------------------------------------------
bool S98File::writeWaitFromPrev(int time)
{
	bool		result;
	
	int		now_time	= time - mDumpBeginTime;
	int		prev_time	= mPrevTime - mDumpBeginTime;
	int		adv_time	= now_time - prev_time;
	
	//�擪�ɋ󔒂�����Δ�΂�
	if ( mPrevTime == mDumpBeginTime ) {
		adv_time = 0;
	}
	
	if ( adv_time == 1 ) {
		result = writeByte( 0xff );	// 1SYNC
		if ( result == false ) return false;
	}
	if ( adv_time > 1 ) {
		adv_time -= 2;		//(n-2)�ŕ\�������
		int	write_len = 0;
		result = writeByte( 0xfe );	// nSYNC
		if ( result == false ) return false;
		write_len++;
		do {
			unsigned char	vv;
			vv = adv_time & 0x7f;
			adv_time >>= 7;
			if ( adv_time > 0 ) {
				vv |= 0x80;		//�܂��c���Ă���Ȃ�p���t���O��ON��
			}
			result = writeByte( vv );
			if ( result == false ) {
				//�������񂾕��������߂�
				mDataPos -= write_len;
				mDataUsed = mDataPos;
				return false;
			}
			write_len++;		//��薳���������߂���P���₷
		} while ( adv_time > 0 );
	}
	mPrevTime = time;
	if ( adv_time < 0 ) {
		mPrevTime -= adv_time;
	}
	return true;
}
