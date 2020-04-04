/*
 *  audioeffectx_intf.h
 *  VOPM
 *
 *  Created by osoumen on 12/09/15.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */
#pragma once

#include "vstgui.h"
#include "AUBase.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#include <stdint.h>
typedef int16_t VstInt16;			///< 16 bit integer type
typedef int32_t VstInt32;			///< 32 bit integer type
typedef int64_t VstInt64;			///< 64 bit integer type
#if __LP64__
typedef VstInt64 VstIntPtr;			///< platform-dependent integer type, same size as pointer
#else
typedef VstInt32 VstIntPtr;			///< platform-dependent integer type, same size as pointer
#endif

#define CCONST(a, b, c, d) \
((((VstInt32)a) << 24) | (((VstInt32)b) << 16) | (((VstInt32)c) << 8) | (((VstInt32)d) << 0))

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
enum VstEventTypes
{
	kVstMidiType = 1,
	kVstSysExType = 6
};

enum VstTimeInfoFlags
{
	kVstPpqPosValid   = 1 << 9,
	kVstTempoValid    = 1 << 10,
};

enum VstPinPropertiesFlags
{
	kVstPinIsActive   = 1 << 0,
	kVstPinIsStereo   = 1 << 1,
	kVstPinUseSpeaker = 1 << 2
};
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct MidiProgramName 
{
	VstInt32 thisProgramIndex;
	char name[64];
	char midiProgram;
	char midiBankMsb;
	char midiBankLsb;
	char reserved;
	VstInt32 parentCategoryIndex;
	VstInt32 flags;
};

struct VstMidiEvent
{
	VstInt32 type;
	VstInt32 byteSize;
	VstInt32 deltaFrames;
	VstInt32 flags;
	VstInt32 noteLength;
	VstInt32 noteOffset;
	char midiData[4];
	char detune;
	char noteOffVelocity;
	char reserved1;
	char reserved2;
};

struct VstTimeInfo
{
	double tempo;
	double ppqPos;
	VstInt32 flags;
};

struct VstEvent
{
	VstInt32 type;
	VstInt32 byteSize;
	VstInt32 deltaFrames;
	VstInt32 flags;
	char data[16];
};

struct VstEvents
{
	VstInt32 numEvents;
	VstIntPtr reserved;
	VstEvent* events[2];
};

struct VstPinProperties
{
	char label[64];
	VstInt32 flags;
	VstInt32 arrangementType;
	char shortLabel[8];
	char future[48];
};
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define audioMasterCallback AUBase*

class PluginGUIEditor;
class AudioEffectXIntf {
public:
	AudioEffectXIntf(AUBase* audioMaster, int numPrograms, int numParams)
	: audiounit( audioMaster ),
	editor( NULL ),
	numPrograms( numPrograms ),
	numParams( numParams )
	{
	}
	virtual ~AudioEffectXIntf() {}
	
	virtual void open () {}
	virtual void close () {}
	virtual void suspend () {}
	virtual void resume () {}
	
	virtual void setBlockSize (VstInt32 blockSize) { this->blockSize = blockSize; }
	virtual void setSampleRate (float sampleRate)  { this->sampleRate = sampleRate; }
	virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) = 0;
	
	virtual void setParameter (VstInt32 index, float value) {}
	virtual float getParameter (VstInt32 index) { return 0; }	
	
	virtual VstInt32 getProgram () { return curProgram; }
	virtual void setProgram (VstInt32 program) { curProgram = program; }
	virtual void setProgramName (char* name) {}
	virtual void getProgramName (char* name) { *name = 0; }
	virtual void getParameterLabel (VstInt32 index, char* label)  { *label = 0; }
	virtual void getParameterDisplay (VstInt32 index, char* text) { *text = 0; }
	virtual void getParameterName (VstInt32 index, char* text)    { *text = 0; }
	virtual VstInt32 getChunk (void** data, bool isPreset = false) { return 0; }
	virtual VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset = false) { return 0; }
	
	virtual VstTimeInfo* getTimeInfo (VstInt32 filter);
	
	virtual void setUniqueID (VstInt32 iD)        { uniqueID = iD; }
	virtual void setNumInputs (VstInt32 inputs)   { numInputs = inputs; }
	virtual void setNumOutputs (VstInt32 outputs) { numOutputs = outputs; }
	virtual void canProcessReplacing () {}
	virtual void programsAreChunks (bool state = true) { programsAreChunksFlag = state; }
	
	virtual void isSynth (bool state = true) { isSynthFlag = state; }
	virtual void* getDirectory () { return NULL; }
	virtual VstIntPtr vendorSpecific (VstInt32 lArg, VstIntPtr lArg2, void* ptrArg, float floatArg) { return 0; }
	
	void setEditor (PluginGUIEditor* editor) { this->editor = editor; }
	virtual PluginGUIEditor* getEditor () { return editor; }
	virtual VstInt32 processEvents (VstEvents* events) { return 0; }

	bool updateDisplay () { return true; }
	
	UInt32 GetNumPrograms() { return numPrograms; }
	UInt32 GetNumParams() { return numParams; }
	
protected:
	AUBase			*audiounit;
	PluginGUIEditor	*editor;
	VstInt32		blockSize;
	float			sampleRate;
	VstInt32		curProgram;
	VstInt32		uniqueID;
	VstInt32		numPrograms;
	VstInt32		numParams;
	VstInt32		numInputs;
	VstInt32		numOutputs;
	bool			programsAreChunksFlag;
	bool			isSynthFlag;
};
