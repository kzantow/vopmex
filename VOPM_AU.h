/*
 *  VOPM_AU.h
 *  VOPM
 *
 *  Created by osoumen on 12/09/15.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */
#pragma once

#include "VOPMVersion.h"
#include "MusicDeviceBase.h"
#include "AUCarbonViewBase.h"
#include "audioeffectx_intf.h"

#if AU_DEBUG_DISPATCHER
#include "AUDebugDispatcher.h"
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class AudioEffectXIntf;

class VOPM_AU : public MusicDeviceBase
{
public:
	VOPM_AU(ComponentInstance inComponentInstance);
	virtual ~VOPM_AU ();
	
	virtual OSStatus			Initialize();

	virtual void				Cleanup();

	virtual OSStatus			Reset(		AudioUnitScope 					inScope,
											AudioUnitElement 				inElement);
/*	
	virtual bool				ValidFormat(			AudioUnitScope					inScope,
														AudioUnitElement				inElement,
														const CAStreamBasicDescription  & inNewFormat);
*/
	
	virtual OSStatus			Version() { return kVOPMVersion; }
	
	virtual bool				StreamFormatWritable(	AudioUnitScope					scope,
														AudioUnitElement				element);

	virtual OSStatus			Render(					AudioUnitRenderActionFlags &	ioActionFlags,
														const AudioTimeStamp &			inTimeStamp,
														UInt32							inNumberFrames);

	
	virtual OSStatus			GetParameter(	AudioUnitParameterID			inID,
												AudioUnitScope 					inScope,
												AudioUnitElement 				inElement,
												AudioUnitParameterValue &		outValue);
	

	virtual OSStatus			SetParameter(	AudioUnitParameterID			inID,
												AudioUnitScope 					inScope,
												AudioUnitElement 				inElement,
												AudioUnitParameterValue			inValue,
												UInt32							inBufferOffsetInFrames);

	virtual OSStatus			GetParameterInfo(AudioUnitScope inScope, AudioUnitParameterID inParameterID, AudioUnitParameterInfo &outParameterInfo);

	virtual OSStatus			SaveState(			CFPropertyListRef *				outData);
	
	virtual OSStatus			RestoreState(		CFPropertyListRef				inData);
	

	
	virtual OSStatus			GetPropertyInfo(	AudioUnitPropertyID				inID,
													AudioUnitScope					inScope,
													AudioUnitElement				inElement,
													UInt32 &						outDataSize,
													Boolean &						outWritable);
	
	virtual OSStatus			GetProperty(		AudioUnitPropertyID 			inID,
													AudioUnitScope 					inScope,
													AudioUnitElement			 	inElement,
													void *							outData);
		
	virtual OSStatus			SetProperty(		AudioUnitPropertyID 			inID,
													AudioUnitScope 					inScope,
													AudioUnitElement 				inElement,
													const void *					inData,
													UInt32 							inDataSize);
	
	virtual ComponentResult		GetPresets(		CFArrayRef	*outData) const;

	virtual OSStatus			NewFactoryPresetSet (const AUPreset & inNewFactoryPreset);

	
	virtual OSStatus			StartNote(		MusicDeviceInstrumentID 	inInstrument, 
												MusicDeviceGroupID 			inGroupID, 
												NoteInstanceID *			outNoteInstanceID, 
												UInt32 						inOffsetSampleFrame, 
												const MusicDeviceNoteParams &inParams);
	
	virtual OSStatus			StopNote(		MusicDeviceGroupID 			inGroupID, 
												NoteInstanceID 				inNoteInstanceID, 
												UInt32 						inOffsetSampleFrame);

	virtual OSStatus	HandleControlChange(	UInt8	inChannel,
												UInt8 	inController,
												UInt8 	inValue,
												UInt32	inStartFrame);
	
	virtual OSStatus	HandlePitchWheel(		UInt8 	inChannel,
												UInt8 	inPitch1,
												UInt8 	inPitch2,
												UInt32	inStartFrame);
	
	virtual OSStatus	HandleChannelPressure(	UInt8 	inChannel,
												UInt8 	inValue,
												UInt32	inStartFrame);
	
	virtual OSStatus	HandleProgramChange(	UInt8	inChannel,
												UInt8 	inValue);
	
	virtual OSStatus	HandlePolyPressure(		UInt8 	inChannel,
												UInt8 	inKey,
												UInt8	inValue,
												UInt32	inStartFrame);
	
	virtual OSStatus	HandleResetAllControllers(		UInt8 	inChannel);
	
	virtual OSStatus	HandleAllNotesOff(				UInt8 	inChannel);
	
	virtual OSStatus	HandleAllSoundOff(				UInt8 	inChannel);
	
	
	//int		GetNumCustomUIComponents () { return 1; }
	
	void	GetUIComponentDescs (ComponentDescription* inDescArray) {
        inDescArray[0].componentType = kAudioUnitCarbonViewComponentType;
        inDescArray[0].componentSubType = VOPM_COMP_SUBTYPE;
        inDescArray[0].componentManufacturer = VOPM_COMP_MANF;
        inDescArray[0].componentFlags = 0;
        inDescArray[0].componentFlagsMask = 0;
	}

	void	UpdateProgramNames();
private:
	AudioEffectXIntf	*mEfx;
	float				mSampleRate;
	float				*mDefaultParams;
	AUPreset			*mPresets;
	
	int ProcessVstMidiEvent( const char *mididata, int bytesize, int offsetFrames );
};
