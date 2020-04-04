//---------------------------------------------------------------------------

#ifndef DISPH
#define DISPH
//---------------------------------------------------------------------------

#include "OPMdrv.hpp"
#include "vstcontrols.h"

class CDisp : public CView{
public:
	~CDisp(void);
	CDisp(CRect &size,OPMDRV *pO);
	void draw (CDrawContext *context);
	void EGBPaint(CDrawContext *context);
	void ConPaint(CDrawContext *context,unsigned char con);
	virtual CMouseEventResult onMouseDown (CPoint &where, const long &button);
	virtual CMouseEventResult onMouseUp (CPoint& where, const long& buttons);
	virtual CMouseEventResult onMouseMoved (CPoint& where, const long& buttons);
private:
	int EGmode;
//	class COffscreenContext *pCOffScreen;
	CBitmap *pConImage;
	CBitmap *pBgImage;
	class OPMDRV *pOPMdrv;
};

#endif
