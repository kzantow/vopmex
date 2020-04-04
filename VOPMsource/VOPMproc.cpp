/*-----------------------------------------------------------------------------
VOPMproc.cpp
2002.3.11 sam
based vstsynthproc.cpp
-----------------------------------------------------------------------------*/

#ifndef __VOPM__
#include "VOPM.hpp"
#endif
#include "OPM.hpp"
#include "OPMdrv.hpp"
#include <math.h>

#if AU
#include "plugguieditor.h"
#else
#include "aeffguieditor.h"
#endif

#include "VOPMEdit.hpp"
#if DEBUG
extern void DebugPrint (char *format, ...);
#endif

#ifdef MAC
	#include <Carbon/Carbon.h>
	#include "globhead.h"
#endif
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
void VOPM::setSampleRate (float sampleRate)
{
	AudioEffectX::setSampleRate (sampleRate);
	SampleRate=(long)sampleRate;
	pOPM->SetSamprate((int)sampleRate);
}

//-----------------------------------------------------------------------------------------
void VOPM::setBlockSize (long blockSize)
{
	AudioEffectX::setBlockSize (blockSize);
	// you may need to have to do something here...
}

//-----------------------------------------------------------------------------------------
void VOPM::resume ()
{
//	wantEvents ();
	AudioEffectX::resume ();
}

//-----------------------------------------------------------------------------------------
void VOPM::suspend ()
{
	pOPMdrv->EndS98Log();
	AudioEffectX::suspend ();
}

//-----------------------------------------------------------------------------------------
void VOPM::initProcess ()
{
char *ptmp;
//char *pbuf;
char filename[512];
char data[32];
long bytesize;
bool Err;
FILE *fin;
		Err=false;
		rate=0;
		SampleRate=44100;
		strcpy(filename,"");
		ptmp=(char *)getDirectory();
		if(ptmp==NULL){
#if MACX
			FSRef outFSRef,parFSRef;
			ProcessSerialNumber  currentProcess = { 0, kCurrentProcess }; 
			GetProcessBundleLocation( &currentProcess, &outFSRef );
			FSGetCatalogInfo(&outFSRef,kFSCatInfoNone,nil,nil,nil,&parFSRef);
			FSRefMakePath(&parFSRef,(UInt8 *)filename,512);
			strcat(filename,"/");	
#else
		strcpy(filename,".\\");
#endif
		}else{
		//Get plugin Directory,Maybe Bug because include plugfilename?
		strcpy(filename,ptmp);
		}

		strcat(filename,"VOPM.fxb");
//		DebugStr((const unsigned char *)filename);
		if((fin=fopen(filename,"rb"))==NULL){
//				DebugStr((const unsigned char *)" couldn't open the file");
				return;
		}
		//Chunk check
		fread(data,sizeof(char),4,fin);
		if(strncmp(data,"CcnK",4)){
		Err=true;
		}
		fseek(fin,4,SEEK_CUR);
		fread(data,sizeof(char),4,fin);
		if(strncmp(data,"FBCh",4)){
		Err=true;
		}
		fseek(fin,4,SEEK_CUR);
		fread(data,sizeof(char),4,fin);
		if(strncmp(data,"VOPM",4)){
		Err=true;
		}
		if(Err){fclose(fin);return;}
		fseek(fin,128+8,SEEK_CUR);
		//calc chunk size
		fread(data,sizeof(char),4,fin);
		bytesize=((unsigned long)data[0]<<24)+
			((unsigned long)data[1]<<16)+
			((unsigned long)data[2]<<8)+
			((unsigned long)data[3]);
		if((ptmp=(char *)malloc(bytesize))==NULL){fclose(fin);return;}
		//read chunk
		fread(ptmp,sizeof(char),bytesize,fin);
		setChunk(ptmp,bytesize,false);
		free(ptmp);
		fclose(fin);
}

//-----------------------------------------------------------------------------------------
void VOPM::process (float **inputs, float **outputs, int sampleFrames)
{
	VstTimeInfo*	info = getTimeInfo(kVstTempoValid | kVstPpqPosValid);
	if ( info ) {
		pOPMdrv->setTempo(info->tempo);
		pOPMdrv->setBeatPos(info->ppqPos);
	}
	
	//波形生成ルーチンを呼ぶ
	pOPM->pcmset22(sampleFrames,outputs,false);
	
	pOPMdrv->FrameTask(sampleFrames);
	
	long fs;
	fs=SampleRate/100;
	rate+=sampleFrames;
	while(rate>fs){
		//10ms毎にAutoSeqを進める
		rate-=fs;
		pOPMdrv->AutoSeq(0);
	}
	
#if DEBUG
	DebugPrint("VOPMproc::process *outputs[0]=%f",
		**outputs
	);
#endif
}

//-----------------------------------------------------------------------------------------
void VOPM::processReplacing (float **inputs, float **outputs, int sampleFrames)
{
	VstTimeInfo*	info = getTimeInfo(kVstTempoValid | kVstPpqPosValid);
	if ( info ) {
		pOPMdrv->setTempo(info->tempo);
		pOPMdrv->setBeatPos(info->ppqPos);
	}
	
	//波形生成ルーチンを呼ぶ
	pOPM->pcmset22(sampleFrames,outputs,true);
	
	pOPMdrv->FrameTask(sampleFrames);
	
	long fs;
	fs=SampleRate/100;
	rate+=sampleFrames;
	while(rate>fs){
		//10ms毎にAutoSeqを進める
		rate-=fs;
		pOPMdrv->AutoSeq(0);
	}
	
#if DEBUG
	DebugPrint("VOPMproc::processReplacing *outputs[0]=%f",
		**outputs
	);
#endif
}

//-----------------------------------------------------------------------------------------
VstInt32 VOPM::processEvents (VstEvents* ev)
{
	for (long i = 0; i < ev->numEvents; i++)
	{
		if ((ev->events[i])->type != kVstMidiType)
			continue;
		VstMidiEvent* event = (VstMidiEvent*)ev->events[i];
		char* midiData = event->midiData;
		pOPMdrv->setDelta((unsigned int)event->deltaFrames);
		unsigned char status = midiData[0] & 0xf0;		// ignoring channel
		char MidiCh = midiData[0] & 0x0f;
		char note = midiData[1] & 0x7f;
		char velocity = midiData[2] & 0x7f;
/*
#if DEBUG
	DebugPrint("VOPMproc::ProcessEvents Message[%02x %02x %02x]",
	midiData[0],midiData[1],midiData[2]);
#endif
*/
		switch(status){
			case 0x80:
				pOPMdrv->NoteOff(MidiCh,note);
				break;
			case 0x90:
				if(velocity==0){
					pOPMdrv->NoteOff(MidiCh,note);
				}else{
					pOPMdrv->NoteOn(MidiCh,note,velocity);
				}
				break;
			case 0xb0:
				if ( pOPMdrv->HandleControlChange(MidiCh, note, velocity) ) {
					if ( note == 0x4C ) {
						//updateDisplay();
						if(editor)((class VOPMEdit *)editor)->update();
					}
				}
				else {
					switch(note) {
						case 118:
							pOPMdrv->MarkS98Loop();
							break;
						case 119:
							if ( velocity == 127 ) {
								pOPMdrv->BeginS98Log();
							}
							else if ( velocity == 0 ) {
								pOPMdrv->EndS98Log();
							}
							break;
							
						default:
							
							if(editor){
								long lTag;
								lTag=((class VOPMEdit *)editor)->getTag();
								if(lTag!=-1){
									lTblCcLearn[note&0x7f]=lTag;
#if DEBUG
									DebugPrint("VOPMproc::learnIn Cc=%x Tag=%d",note,lTag);
#endif
								}else{
									lTag=lTblCcLearn[note&0x7f];
#if DEBUG
									DebugPrint("VOPMproc::learnOut Cc=%x Tag=%d",note,lTag);
#endif
								}
								
								setParameter((VstInt32)lTag,(float)velocity/127.0);
#if DEBUG
								DebugPrint("VOPMproc::defaultCC CcNo=%x tag=%d float=%f",note,lTag,(float)velocity/127.0);
#endif
								((class VOPMEdit *)editor)->update();
							}
							
					}
				}
				break;
			case 0xc0:
				pOPMdrv->VoCng(MidiCh,note);
				break;
			case 0xe0:
				pOPMdrv->BendCng(MidiCh,(velocity<<7)+note);
				break;
		}
		//event++;
	}
	return 1;	// want more
}

//-----------------------------------------------------------------------------------------

