/*
 *  VOPM_AUView.cpp
 *  VOPM
 *
 *  Created by osoumen on 12/09/15.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#include "VOPM_AUView.h"
#include "VOPMEdit.hpp"

COMPONENT_ENTRY(VOPM_AUView)


void VOPM_AUView::RespondToEventTimer (EventLoopTimerRef inTimer) 
{
	if (editor) {
		editor->doIdleStuff ();
	}
}

OSStatus VOPM_AUView::CreateUI(Float32 xoffset, Float32 yoffset)
{
	AudioUnit unit = GetEditAudioUnit ();
	if (unit) {
		VOPM	*efx;
		UInt32			size = sizeof(AudioEffectX*);
		
		AudioUnitGetProperty(unit, 64000, kAudioUnitScope_Global, 0, (void*)&efx, &size);
		
		CFrame::setCocoaMode(false);
		
		editor = efx->getEditor();
		WindowRef window = GetCarbonWindow ();
		editor->open (window);
//		HIViewMoveBy ((HIViewRef)editor->getFrame ()->getPlatformControl (), xoffset, yoffset);
		EmbedControl ((HIViewRef)editor->getFrame ()->getPlatformControl ());
		CRect fsize = editor->getFrame ()->getViewSize (fsize);
		SizeControl (mCarbonPane, fsize.width (), fsize.height ());
		CreateEventLoopTimer (kEventDurationSecond, kEventDurationSecond / 24);
		HIViewSetVisible ((HIViewRef)editor->getFrame ()->getPlatformControl (), true);
		HIViewSetNeedsDisplay ((HIViewRef)editor->getFrame ()->getPlatformControl (), true);
	}
	return noErr;
}
