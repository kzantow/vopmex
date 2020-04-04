/*
 *  VOPM_AUView.h
 *  VOPM
 *
 *  Created by osoumen on 12/09/15.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#include "AUCarbonViewBase.h"

#if AU
#include "plugguieditor.h"
#else
#include "aeffguieditor.h"
#endif

class VOPM_AUView : public AUCarbonViewBase 
{
public:
	VOPM_AUView (AudioUnitCarbonView auv) 
	: AUCarbonViewBase (auv)
	, xOffset (0)
	, yOffset (0)
	, editor (0)
	{
	}

	virtual ~VOPM_AUView ()
	{
		if (editor)
		{
			editor->close ();
		}
	}
	
	void RespondToEventTimer (EventLoopTimerRef inTimer);

	virtual OSStatus CreateUI(Float32 xoffset, Float32 yoffset);

	Float32 xOffset, yOffset;
protected:
	AEffGUIEditor	*editor;
};
