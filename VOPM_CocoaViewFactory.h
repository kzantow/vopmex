/*
 *  VOPM_CocoaViewFactory.h
 *  VOPM
 *
 *  Created by osoumen on 12/09/15.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#import <Cocoa/Cocoa.h>
#import <AudioUnit/AUCocoaUIView.h>

#if AU
#include "plugguieditor.h"
#else
#include "aeffguieditor.h"
#endif

@class VOPM_CocoaView;


@interface VOPM_CocoaViewFactory : NSObject <AUCocoaUIBase>
{
}

- (NSString *) description;	// string description of the view

@end

@interface VOPM_CocoaView : NSView
{
	AEffGUIEditor	*editor;
	NSTimer			*timer;
}

- (VOPM_CocoaView *)initWithFrame:(NSRect)frameRect audioUnit:(AudioUnit)inAU;
- (void)setEditorFrame;

@end
