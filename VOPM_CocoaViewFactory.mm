/*
 *  VOPM_CocoaViewFactory.mm
 *  VOPM
 *
 *  Created by osoumen on 12/09/15.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#import "VOPM_CocoaViewFactory.h"
#include "VOPMEdit.hpp"

@implementation VOPM_CocoaViewFactory

// version 0
- (unsigned) interfaceVersion {
	return 0;
}

// string description of the Cocoa UI
- (NSString *) description {
	return @" Cocoa View";
}

- (void)dealloc {
	[super dealloc];
}

- (NSView *)uiViewForAudioUnit:(AudioUnit)inAU withSize:(NSSize)inPreferredSize
{
	NSRect r;
	r.origin.x = 0;
	r.origin.y = 0;
	r.size.width = 200;
	r.size.height = 200;
	
    VOPM_CocoaView *returnView = [[VOPM_CocoaView alloc] initWithFrame:r audioUnit:inAU];
	[returnView setEditorFrame];
	
    return [returnView autorelease];
}

@end


@implementation VOPM_CocoaView

- (VOPM_CocoaView*)initWithFrame:(NSRect)frameRect audioUnit:(AudioUnit)inAU
{
	VOPM_CocoaView	*view = [super initWithFrame:frameRect];
	
	VOPM		*efx;
	UInt32		size = sizeof(AudioEffectX*);
	
	AudioUnitGetProperty(inAU, 64000, kAudioUnitScope_Global, 0, (void*)&efx, &size);
#ifndef __LP64__
	CFrame::setCocoaMode(true);
#endif
	editor = efx->getEditor();
	if ( editor ) {
		editor->open(self);
	}
	
	//タイマーの設定
	timer = [NSTimer scheduledTimerWithTimeInterval:1.0/24
											 target:self
										   selector:@selector(respondToEventTimer:)
										   userInfo:nil
											repeats:YES];	
	
	return view;
}

- (void)respondToEventTimer:(NSTimer*)inTimer
{
	if (editor) {
		editor->doIdleStuff ();
	}
}

- (void)setEditorFrame
{
	CRect fsize;
	if ( editor ) {
		fsize = editor->getFrame ()->getViewSize (fsize);
	}
	NSRect r;
	r.origin.x = fsize.left;
	r.origin.y = fsize.top;
	r.size.width = fsize.getWidth ();
	r.size.height = fsize.getHeight ();
	[self setFrame:r];	
}

- (void)dealloc
{
	//タイマーの停止
	if ( timer ) {
		[timer invalidate];
	}
	
	if (editor) {
		editor->close ();
	}
	[super dealloc];
}

@end
