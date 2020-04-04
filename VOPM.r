/*
 *  VOPM.r
 *  VOPM
 *
 *  Created by osoumen on 12/09/15.
 *  Copyright 2012 __MyCompanyName__. All rights reserved.
 *
 */

#include <AudioUnit/AudioUnit.r>

#include "VOPMVersion.h"

// Note that resource IDs must be spaced 2 apart for the 'STR ' name and description
#define kAudioUnitResID_VOPM				1000
#define kAudioUnitResID_VOPMView			2000

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ VOPM~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define RES_ID			kAudioUnitResID_VOPM
#define COMP_TYPE		kAudioUnitType_MusicDevice
#define COMP_SUBTYPE	VOPM_COMP_SUBTYPE
#define COMP_MANUF		VOPM_COMP_MANF	

#define VERSION			kVOPMVersion
#define NAME			"Sam: VOPMex"
#define DESCRIPTION		"VOPM AU"
#define ENTRY_POINT		"VOPM_AUEntry"

#include "AUResources.r"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ VOPMView~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*
#define RES_ID			kAudioUnitResID_VOPMView
#define COMP_TYPE		kAudioUnitCarbonViewComponentType
#define COMP_SUBTYPE	VOPM_COMP_SUBTYPE
#define COMP_MANUF		VOPM_COMP_MANF	

#define VERSION			kVOPMVersion
#define NAME			"Sam: VOPM"
#define DESCRIPTION		"VOPM Carbon AU View"
#define ENTRY_POINT		"VOPM_AUViewEntry"

#include "AUResources.r"
*/
