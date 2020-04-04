/*
 *  VOPM_AU.cpp
 *  VOPM
 *
 *  Created by osoumen on 12/09/15.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

/*=============================================================================
	VOPM_AU.cpp
	
 =============================================================================*/

#include "VOPM_AU.h"
#include "VOPM.hpp"
#include "plugguieditor.h"

static void AddNumToDictionary (CFMutableDictionaryRef dict, CFStringRef key, SInt32 value)
{
	CFNumberRef num = CFNumberCreate (NULL, kCFNumberSInt32Type, &value);
	CFDictionarySetValue (dict, key, num);
	CFRelease (num);
}

static CFStringRef kSaveKey_EditProg = CFSTR("editprog");

VstTimeInfo* AudioEffectXIntf::getTimeInfo (VstInt32 filter)
{
	static VstTimeInfo timeInfo;
	Float64		tempo;
	Float64		ppqPos;
	
	audiounit->CallHostBeatAndTempo(&ppqPos, &tempo);
	
	if ( filter & kVstTempoValid ) {
		timeInfo.tempo = tempo;
	}
	if ( filter & kVstPpqPosValid ) {
		timeInfo.ppqPos = ppqPos;
	}
	return &timeInfo;
}

COMPONENT_ENTRY(VOPM_AU)

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::VOPM_AU
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
VOPM_AU::VOPM_AU(ComponentInstance inComponentInstance)
	: MusicDeviceBase(inComponentInstance, 0, 1, 32, 0)
{
	CreateElements();	
	
	//VSTインスタンスの作成
	mEfx = new VOPM(this);
	
	//パラメータ数の設定
	Globals()->UseIndexedParameters(mEfx->GetNumParams());
	
	//最大ブロックサイズの設定
	mEfx->setBlockSize(kAUDefaultMaxFramesPerSlice);
	//サンプリングレートの設定
	mSampleRate = GetOutput(0)->GetStreamFormat().mSampleRate;;
	mEfx->setSampleRate(mSampleRate);
	
	//プリセット名テーブルを作成する
	mPresets = new AUPreset[mEfx->GetNumPrograms()];
	for (UInt32 i = 0; i < mEfx->GetNumPrograms(); ++i) {
		char		pname[32];
		mEfx->setProgram(i);
		mEfx->getProgramName(pname);
		mPresets[i].presetNumber = i;
		CFMutableStringRef cfpname = CFStringCreateMutable(NULL, 64);
		mPresets[i].presetName = cfpname;
		CFStringAppendCString( cfpname, pname, kCFStringEncodingASCII );
    }
	mEfx->setProgram(0);
	
	//パラメータ初期値の取得
	mDefaultParams = new float[mEfx->GetNumParams()];
	for ( UInt32 i=0; i<mEfx->GetNumParams(); i++ ) {
		mDefaultParams[i] = mEfx->getParameter( i );
		Globals()->SetParameter(i, mDefaultParams[i]);
	}
	
#if AU_DEBUG_DISPATCHER
	mDebugDispatcher = new AUDebugDispatcher(this);
#endif
}

VOPM_AU::~VOPM_AU()
{
	delete [] mDefaultParams;
	
	for (UInt32 i = 0; i < mEfx->GetNumPrograms(); ++i) {
		CFRelease(mPresets[i].presetName);
	}
	delete [] mPresets;
	delete mEfx;
	
#if AU_DEBUG_DISPATCHER
	delete mDebugDispatcher;
#endif
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::Initialize
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::Initialize()
{	
	MusicDeviceBase::Initialize();	
	mEfx->open();
	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::Cleanup
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void VOPM_AU::Cleanup()
{
	mEfx->close();
	MusicDeviceBase::Cleanup();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::Reset
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::Reset(	AudioUnitScope 					inScope,
							AudioUnitElement 				inElement)
{
#if DEBUG_PRINT
	printf("VOPM_AU::Reset\n");
#endif
	if (inScope == kAudioUnitScope_Global)
	{
		//サンプルレートの設定
		float samplerate = GetOutput(0)->GetStreamFormat().mSampleRate;
		if ( mSampleRate != samplerate ) {
			mSampleRate = samplerate;
			mEfx->setSampleRate(samplerate);
		}
		
		mEfx->suspend();
		mEfx->resume();
		// kill all notes..
	}
	return MusicDeviceBase::Reset(inScope, inElement);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::ValidFormat
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*
bool VOPM_AU::ValidFormat(	AudioUnitScope		inScope,
							AudioUnitElement				inElement,
							const CAStreamBasicDescription  & inNewFormat)
{	
	// if the AU supports this, then we should just let this go through to the Init call
	if (SupportedNumChannels (NULL)) 
		return MusicDeviceBase::ValidFormat(inScope, inElement, inNewFormat);
	
	bool isGood = MusicDeviceBase::ValidFormat (inScope, inElement, inNewFormat);
	if (!isGood) return false;
	
	// if we get to here, then the basic criteria is that the
	// num channels cannot change on an existing bus
	AUIOElement *el = GetIOElement (inScope, inElement);
	return (el->GetStreamFormat().NumberChannels() == inNewFormat.NumberChannels()); 
}
*/

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::StreamFormatWritable
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool VOPM_AU::StreamFormatWritable(	AudioUnitScope					scope,
									AudioUnitElement				element)
{
	return IsInitialized() ? false : true;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::Render
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::Render(   AudioUnitRenderActionFlags &	ioActionFlags,
							const AudioTimeStamp &			inTimeStamp,
							UInt32							inNumberFrames)
{
	/*
	Float64				outCurrentBeat;
	Float64				outCurrentTempo;
	if ( CallHostBeatAndTempo(&outCurrentBeat, &outCurrentTempo) != -1 ) {
		printf("outCurrentBeat=%lf\n",outCurrentBeat);
		printf("outCurrentTempo=%lf\n",outCurrentTempo);
	}
	
	
	UInt32  		outDeltaSampleOffsetToNextBeat;
	Float32         outTimeSig_Numerator;
	UInt32          outTimeSig_Denominator;
	Float64         outCurrentMeasureDownBeat;
	if ( CallHostMusicalTimeLocation(&outDeltaSampleOffsetToNextBeat,
									   &outTimeSig_Numerator,
									   &outTimeSig_Denominator,
									   &outCurrentMeasureDownBeat) != -1 ) {
		printf("outDeltaSampleOffsetToNextBeat=%d\n",outDeltaSampleOffsetToNextBeat);
		printf("outTimeSig_Numerator=%f\n",outTimeSig_Numerator);
		printf("outTimeSig_Denominator=%d\n",outTimeSig_Denominator);
		printf("outCurrentMeasureDownBeat=%lf\n",outCurrentMeasureDownBeat);
	}
	
	Float64 		outCurrentSampleInTimeLine;
	if ( CallHostTransportState(NULL,NULL,&outCurrentSampleInTimeLine,NULL,NULL,NULL) != -1 ) {
		printf("outCurrentSampleInTimeLine=%lf\n",outCurrentSampleInTimeLine);
	}
	*/
	//サンプルレートの設定
	float samplerate = GetOutput(0)->GetStreamFormat().mSampleRate;
	if ( mSampleRate != samplerate ) {
		mSampleRate = samplerate;
		mEfx->setSampleRate(samplerate);
	}
	
	//バッファの確保
	float				*output[2];
	AudioBufferList&	bufferList = GetOutput(0)->GetBufferList();
	
	int numChans = bufferList.mNumberBuffers;
	if (numChans > 2) return -1;
	output[0] = (float*)bufferList.mBuffers[0].mData;
	output[1] = numChans==2 ? (float*)bufferList.mBuffers[1].mData : NULL;
	
	mEfx->processReplacing(output, output, inNumberFrames);
	
	return noErr;
}
//_____________________________________________________________________________
//
OSStatus VOPM_AU::GetParameter(	AudioUnitParameterID			inID,
								AudioUnitScope 					inScope,
								AudioUnitElement 				inElement,
								AudioUnitParameterValue &		outValue)
{
	OSStatus err = MusicDeviceBase::GetParameter( inID, inScope, inElement, outValue );
	return err;
}


//_____________________________________________________________________________
//
OSStatus VOPM_AU::SetParameter(	AudioUnitParameterID			inID,
								AudioUnitScope 					inScope,
								AudioUnitElement 				inElement,
								AudioUnitParameterValue			inValue,
								UInt32							inBufferOffsetInFrames)
{
	if ( inScope == kAudioUnitScope_Global ) {
		mEfx->setParameter(inID, inValue);
		if ( mEfx->getEditor () ) {
			mEfx->getEditor()->setParameter(inID, inValue);
		}
	}
	OSStatus err = MusicDeviceBase::SetParameter( inID, inScope, inElement, inValue, inBufferOffsetInFrames );
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::GetParameterInfo
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::GetParameterInfo(AudioUnitScope inScope, AudioUnitParameterID inParameterID, AudioUnitParameterInfo &outParameterInfo)
{
	if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
	
	if ( inParameterID < 0 || inParameterID >= mEfx->GetNumParams() ) {
		return kAudioUnitErr_InvalidParameter;
	}

    outParameterInfo.flags = kAudioUnitParameterFlag_IsWritable;
	outParameterInfo.flags += kAudioUnitParameterFlag_IsReadable;

	char cname[10];		//現在全て8文字以内
	mEfx->getParameterName(inParameterID, cname);
	CFStringRef	cfName = CFStringCreateWithCString(NULL, cname, kCFStringEncodingASCII);
	AUBase::FillInParameterName(outParameterInfo, cfName, true);
	outParameterInfo.unit = kAudioUnitParameterUnit_Generic;
	outParameterInfo.minValue = 0;
	outParameterInfo.maxValue = 1.0; //VSTの仕様
	outParameterInfo.defaultValue = mDefaultParams[inParameterID];		//コンストラクタで取得
	
	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::SaveState
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult	VOPM_AU::SaveState(CFPropertyListRef *outData)
{
	//内部設定値をパラメータに反映させる
	for ( UInt32 i=0; i<mEfx->GetNumParams(); i++ ) {
		Globals()->SetParameter(i, mEfx->getParameter(i));
	}
	
	ComponentResult result;
	result = MusicDeviceBase::SaveState(outData);
	
	CFMutableDictionaryRef	dict=(CFMutableDictionaryRef)*outData;
	if (result == noErr) {
		// チャンクデータの保存
		CFDataRef	pgdata;
		void		*data;
		UInt32		dataSize;
		
		dataSize = mEfx->getChunk(&data, false);
		pgdata = CFDataCreate(NULL, (UInt8*)data, dataSize);
		CFDictionarySetValue(dict, CFSTR(kAUPresetVSTDataKey), pgdata);
		CFRelease(pgdata);
		
		// プログラム名の保存
		CFStringRef		pgname;
		char			cpgname[16];
		mEfx->getProgramName(cpgname);
		pgname = CFStringCreateWithCString(NULL, cpgname, kCFStringEncodingASCII);
		CFDictionarySetValue(dict, CFSTR(kAUPresetNameKey), pgname);
		CFRelease(pgname);
		
		// 選択プログラムの保存
		AddNumToDictionary(dict, kSaveKey_EditProg, mEfx->getProgram());
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::RestoreState
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult	VOPM_AU::RestoreState(CFPropertyListRef plist)
{
	CFDictionaryRef dict = static_cast<CFDictionaryRef>(plist);
	if (dict) {
		// チャンクデータの復元
		CFDataRef	pgdata;	
		pgdata = reinterpret_cast<CFDataRef>(CFDictionaryGetValue(dict, CFSTR(kAUPresetVSTDataKey)));
		if ( pgdata ) {
			const void		*data = CFDataGetBytePtr(pgdata);
			UInt32			dataSize;
			dataSize = CFDataGetLength(pgdata);
			mEfx->setChunk((void *)data, dataSize, false);
		}
		
		// プリセット名を反映させる
		UpdateProgramNames();
		
		// 選択プログラムの復元
		if (CFDictionaryContainsKey(dict, kSaveKey_EditProg)) {
			CFNumberRef cfnum = reinterpret_cast<CFNumberRef>(CFDictionaryGetValue(dict, kSaveKey_EditProg));
			int	program;
			CFNumberGetValue(cfnum, kCFNumberIntType, &program);
			mEfx->setProgram(program);
		}
	}
	
	ComponentResult result;
	result = MusicDeviceBase::RestoreState(plist);
	
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::GetPropertyInfo
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult VOPM_AU::GetPropertyInfo (	AudioUnitPropertyID	inID,
											AudioUnitScope		inScope,
											AudioUnitElement	inElement,
											UInt32 &			outDataSize,
											Boolean &			outWritable)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
			
			case kAudioUnitProperty_CocoaUI:
				outWritable = false;
				outDataSize = sizeof (AudioUnitCocoaViewInfo);
				return noErr;
			
			case 64000:		//test
				outDataSize = sizeof(AudioEffectXIntf*);
				outWritable = false;
				return noErr;
		}
	}
	return MusicDeviceBase::GetPropertyInfo(inID, inScope, inElement, outDataSize, outWritable);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::GetProperty
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult	VOPM_AU::GetProperty(	AudioUnitPropertyID inID,
										AudioUnitScope 		inScope,
										AudioUnitElement 	inElement,
										void *				outData )
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
			case kAudioUnitProperty_CocoaUI:
			{
				// Look for a resource in the main bundle by name and type.
				CFBundleRef bundle = CFBundleGetBundleWithIdentifier( CFSTR("com.Sam.audiounit.VOPM") );
				
				if (bundle == NULL) return fnfErr;
				/*
				CFURLRef bundleURL = CFBundleCopyResourceURL( bundle, 
															 CFSTR("VOPM"), 
															 CFSTR("component"), 
															 NULL);
				*/
				CFURLRef bundleURL = CFBundleCopyBundleURL( bundle );
				
				if (bundleURL == NULL) return fnfErr;
				
				AudioUnitCocoaViewInfo cocoaInfo;
				cocoaInfo.mCocoaAUViewBundleLocation = bundleURL;
				cocoaInfo.mCocoaAUViewClass[0] = CFStringCreateWithCString(NULL, "VOPM_CocoaViewFactory", kCFStringEncodingUTF8);
				
				*((AudioUnitCocoaViewInfo *)outData) = cocoaInfo;
				
				return noErr;
			}
			
			case 64000:		//test
				*((AudioEffectX**)outData) = mEfx;
				return noErr;
		}
	}
	
	return MusicDeviceBase::GetProperty(inID, inScope, inElement, outData);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::SetProperty
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult	VOPM_AU::SetProperty(	AudioUnitPropertyID inID,
										AudioUnitScope 		inScope,
										AudioUnitElement 	inElement,
										const void *		inData,
										UInt32              inDataSize)
{
	if (inScope == kAudioUnitScope_Global) {
		switch (inID) {
			case 64000:		//test
				return noErr;
		}
	}
	return MusicDeviceBase::SetProperty(inID, inScope, inElement, inData, inDataSize);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::GetPresets
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult VOPM_AU::GetPresets(CFArrayRef *outData) const
{	
#if DEBUG_PRINT
	printf("VOPM_AU::GetPresets\n");
#endif
	if (outData == NULL) return noErr;
	
	//プリセット名の配列を渡す
	CFMutableArrayRef theArray = CFArrayCreateMutable(NULL, mEfx->GetNumPrograms(), NULL);
	for (UInt32 i = 0; i < mEfx->GetNumPrograms(); ++i) {
		CFArrayAppendValue(theArray, &mPresets[i]);
    }
	*outData = (CFArrayRef)theArray;
	
	return noErr;
}

OSStatus VOPM_AU::NewFactoryPresetSet(const AUPreset &inNewFactoryPreset)
{
#if DEBUG_PRINT
	printf("VOPM_AU::NewFactoryPresetSet\n");
#endif
	UInt32 chosenPreset = inNewFactoryPreset.presetNumber;
//printf("chosenPreset=%d\n",chosenPreset);
	if ( chosenPreset < mEfx->GetNumPrograms() ) {
		mEfx->setProgram(chosenPreset);
		char cname[32];
		CFStringGetCString(inNewFactoryPreset.presetName, cname, 32, kCFStringEncodingASCII);
		mEfx->setProgramName(cname);
		SetAFactoryPresetAsCurrent(inNewFactoryPreset);
		for ( UInt32 i=0; i<mEfx->GetNumParams(); i++ ) {
			Globals()->SetParameter(i, mEfx->getParameter(i));
		}
		return noErr;
	}
	return kAudioUnitErr_InvalidPropertyValue;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int VOPM_AU::ProcessVstMidiEvent( const char *mididata, int bytesize, int offsetFrames )
{
	VstEvents		ev;
	VstMidiEvent	event;
	
	memset(&ev, 0, sizeof(VstEvents));
	memset(&event, 0, sizeof(VstMidiEvent));
	
	char* data = event.midiData;
	
	for ( int i=0; i<bytesize; i++ ) {
		data[i] = mididata[i];
	}
	
	event.byteSize = bytesize;
	event.type = kVstMidiType;
	event.deltaFrames = offsetFrames;
	
	ev.numEvents = 1;
	ev.events[0] = (VstEvent*)&event;
	
	return mEfx->processEvents(&ev);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::StartNote
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::StartNote(	MusicDeviceInstrumentID 	inInstrument, 
								MusicDeviceGroupID 			inGroupID, 
								NoteInstanceID *			outNoteInstanceID, 
								UInt32 						inOffsetSampleFrame, 
								const MusicDeviceNoteParams &inParams)
{
#if DEBUG_PRINT
	printf("VOPM_AU::StartNote %d\n", inGroupID);
#endif
	OSStatus err = noErr;
	
	char midiData[3];
	midiData[0] = 0x90 + (inGroupID & 0x0f);	// MIDI NoteOn + ch
	midiData[1] = inParams.mPitch;
	midiData[2] = inParams.mVelocity;
	ProcessVstMidiEvent( midiData, sizeof(midiData), inOffsetSampleFrame );
	
	return err;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::StopNote
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::StopNote(	MusicDeviceGroupID 			inGroupID, 
							NoteInstanceID 				inNoteInstanceID, 
							UInt32 						inOffsetSampleFrame)
{
#if DEBUG_PRINT
	printf("VOPM_AU::StopNote %d %d\n", inGroupID, inNoteInstanceID);
#endif
	OSStatus err = noErr;

	char midiData[3];
	midiData[0] = 0x80 + (inGroupID & 0x0f);	// MIDI NoteOff + ch
	midiData[1] = inNoteInstanceID & 0x7f;
	midiData[2] = 64;
	ProcessVstMidiEvent( midiData, sizeof(midiData), inOffsetSampleFrame );
	
	return err;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::HandleControlChange
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::HandleControlChange(	UInt8 	inChannel,
										UInt8 	inController,
										UInt8 	inValue,
										UInt32	inStartFrame)
{
	char midiData[3];
	midiData[0] = 0xb0 + (inChannel & 0x0f);	// MIDI CC + ch
	midiData[1] = inController & 0x7f;
	midiData[2] = inValue & 0x7f;
	ProcessVstMidiEvent( midiData, sizeof(midiData), inStartFrame );

	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::HandlePitchWheel
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::HandlePitchWheel(		UInt8 	inChannel,
										UInt8 	inPitch1,
										UInt8 	inPitch2,
										UInt32	inStartFrame)
{
	char midiData[3];
	midiData[0] = 0xe0 + (inChannel & 0x0f);	// MIDI PitchBend + ch
	midiData[1] = inPitch1 & 0x7f;
	midiData[2] = inPitch2 & 0x7f;
	ProcessVstMidiEvent( midiData, sizeof(midiData), inStartFrame );
	
	return noErr;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::HandleChannelPressure
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::HandleChannelPressure(UInt8 	inChannel,
										UInt8 	inValue,
										UInt32	inStartFrame)
{
	return noErr;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::HandleProgramChange
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::HandleProgramChange(	UInt8 	inChannel,
										UInt8 	inValue)
{
	char midiData[2];
	midiData[0] = 0xc0 + (inChannel & 0x0f);	// MIDI ProgramChange + ch
	midiData[1] = inValue & 0x7f;
	ProcessVstMidiEvent( midiData, sizeof(midiData), 0 );
	
	return noErr;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::HandlePolyPressure
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::HandlePolyPressure(	UInt8 	inChannel,
										UInt8 	inKey,
										UInt8	inValue,
										UInt32	inStartFrame)
{
	return noErr;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::HandleResetAllControllers
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::HandleResetAllControllers(	UInt8 	inChannel)
{
	char midiData[3];
	midiData[0] = 0xb0 + (inChannel & 0x0f);	// MIDI CC + ch
	midiData[1] = 0x79;
	midiData[2] = 127;
	ProcessVstMidiEvent( midiData, sizeof(midiData), 0 );
	
	return noErr;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::HandleAllNotesOff
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::HandleAllNotesOff(	UInt8 	inChannel)
{
	char midiData[3];
	midiData[0] = 0xb0 + (inChannel & 0x0f);	// MIDI CC + ch
	midiData[1] = 0x7b;
	midiData[2] = 127;
	ProcessVstMidiEvent( midiData, sizeof(midiData), 0 );
	
	return noErr;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::HandleAllSoundOff
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus VOPM_AU::HandleAllSoundOff(	UInt8 	inChannel)
{
	char midiData[3];
	midiData[0] = 0xb0 + (inChannel & 0x0f);	// MIDI CC + ch
	midiData[1] = 0x78;
	midiData[2] = 127;
	ProcessVstMidiEvent( midiData, sizeof(midiData), 0 );
	
	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	VOPM_AU::UpdatePragramNames
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void VOPM_AU::UpdateProgramNames()
{
	int oldProg = mEfx->getProgram();
	
	for (UInt32 i = 0; i < mEfx->GetNumPrograms(); ++i) {
		char		pname[32];
		mEfx->setProgram(i);
		mEfx->getProgramName(pname);
		CFStringRef	newname = CFStringCreateWithCString(NULL, pname, kCFStringEncodingASCII);
		CFStringReplaceAll( (CFMutableStringRef)mPresets[i].presetName, newname );
		CFRelease(newname);
	}
	
	mEfx->setProgram(oldProg);
}
