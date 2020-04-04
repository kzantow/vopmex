#include "VOPM.hpp"
#include "vstgui.h"
#include "OPMdrv.hpp"

#if AU
#include "vstkeycode.h"
#endif

#ifndef __SLIDH__
#define __SLIDH__



class CDispSlider: public CControl
{
public:
	~CDispSlider();
	CDispSlider (const CRect &size, CControlListener *listener, long tag,
	float	Max,
	float	Min,
	float Pos,
	CBitmap *handle,	 // handle bitmap
	CBitmap *background); // background bitmap

	virtual bool attached (CView *parent);
	virtual bool removed (CView *parent);
	virtual void draw (CDrawContext*);
	virtual CMouseEventResult onMouseDown (CPoint &where, const long &button);
	virtual CMouseEventResult onMouseUp (CPoint& where, const long& buttons);
	virtual CMouseEventResult onMouseMoved (CPoint& where, const long& buttons);
	virtual bool onWheel (const CPoint &where, const float &distance, const long &buttons);
	virtual long onKeyDown (VstKeyCode& keyCode);

	virtual void setDrawTransparentHandle (bool val) { bDrawTransparentEnabled = val; }


	virtual void	 setHandle (CBitmap* pHandle);
	virtual CBitmap *getHandle () { return pHandle; }
	void setHexText(bool stat);
	void setLearn(bool stat);

	CLASS_METHODS(CDispSlider, CControl)
	
protected:
	CBitmap *pHandle;
	COffscreenContext *pOScreen;

	long	 widthOfSlider; // size of the handle-slider
	long	 heightOfSlider;

	float	rangeHandle;

	long	minPos;
	long	maxPos;
	long	widthControl;
	long	heightControl;

	bool	bDrawTransparentEnabled;
	bool	bHexText;
	bool	bLearn;
	
private:
	int		cnt;
	float	oldVal;
};

#define SLID_SLID 0
#define SLID_SEG 1

#define SLID_HEIGHT 32	//slider length(bit)
#define SLID_GAP 2			//Gap,slider to 7SEG(bit) 
#endif
