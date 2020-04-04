/*
 *  ringbuff.h
 *  RingBuffer module
 *  c86ctlÇ…ä‹Ç‹ÇÍÇÈhonetéÅçÏê¨ÇÃÇ‡ÇÃÇâ¸ïœÇµÇƒÇ¢Ç‹Ç∑ÅB
 *
 *  Created by osoumen on 12/09/15.
 *
 *  Copyright (c) 2009-2012, honet. All rights reserved.
 *  This software is licensed under the BSD license.
 *
 *  honet.kk(at)gmail.com
 * 
 */

#ifndef RINGBUFF_H_
#define RINGBUFF_H_

#if __APPLE_CC__
#include <libkern/OSAtomic.h>
#endif

#if _WIN32
#include <windows.h>
#endif

template <class T>
class CRingBuff {
protected:
	T *p;
	unsigned int sz;
	unsigned int mask;
#if _WIN32
	volatile long widx;
	volatile long ridx;
#else
	volatile int widx;
	volatile int ridx;
#endif
	
public:
	CRingBuff(){
		p = NULL;
		sz = 0;
		mask = 0;
		widx = 0;
		ridx = 0;
	};

	~CRingBuff(){
		freeres();
	};
	
	bool alloc( unsigned int asize ){
		freeres();
		p = new T[asize];
		if( p ){
			sz = asize;
			mask = sz - 1;
			widx = 0;
			ridx = 0;
		}
		return p ? true : false;
	};

	void freeres(void){
		if( p )
			delete [] p;

		p = NULL;
		sz = 0;
		mask = 0;
		widx = 0;
		ridx = 0;
	};

	unsigned int remain(void){
		if( !p ) return 0;
		unsigned int cridx = ridx & mask;
		unsigned int cwidx = widx & mask;
		if( cridx <= cwidx ){
			return cridx + (sz - cwidx) - 1;
		}else{
			return cridx - cwidx - 1;
		}
	};

	unsigned int get_length(void){
		if( !p ) return 0;
		unsigned int cridx = ridx & mask;
		unsigned int cwidx = widx & mask;
		if( cridx <= cwidx ){
			return cwidx - cridx;
		}else{
			return cwidx + (sz - cridx);
		}
	};

	bool isempty(void){
		if( !p ) return 0;
		return ( get_length() == 0 );
	}

	T* query_read_ptr(void){
		if( !p )
			return 0;
		
		return p + (ridx&mask);
	};
	
	T* idx_read_ptr(int idx){
		if( !p )
			return 0;
		
		return p + ((ridx+idx)&mask);
	};
	
	bool read1( T *data ){
		if( !p )
			return false;

		*data = *(p + (ridx&mask));
#ifdef _WIN32
		::InterlockedIncrement(&ridx);
#else
		OSAtomicIncrement32(&ridx);
#endif
		return true;
	};
	
	bool read_idx( T *data, int idx ){
		if( !p )
			return false;
		
		*data = *(p + ((ridx+idx)&mask));
		return true;
	};
	
	bool adv_ridx(){
		if( !p )
			return false;
		
#ifdef _WIN32
		::InterlockedIncrement(&ridx);
#else
		OSAtomicIncrement32(&ridx);
#endif
		return true;
	};
	
	void write1( T data ){
		if( !p ) return;
		while( remain() < 1 ){
#ifdef WIN32
			Sleep(1);
#else
			usleep(1*1000);
#endif		
		}

		T *pd = p + (widx&mask);
		*pd = data;
#ifdef _WIN32
		::InterlockedIncrement(&widx);
#else
		OSAtomicIncrement32(&widx);
#endif
	};

};

#endif

