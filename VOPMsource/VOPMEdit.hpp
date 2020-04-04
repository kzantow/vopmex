//VOPMEdit.hpp
//
//
#ifndef __OPMDRV__
#include "OPMdrv.hpp"
#endif

#if AU
#include "plugguieditor.h"
#else
#include "aeffguieditor.h"
#endif

#include "VOPM.hpp"
#include "vstgui.h"

class VOPMEdit : public AEffGUIEditor
{
public:
	VOPMEdit(VOPM *effect);
	virtual ~VOPMEdit();
	virtual bool getRect(ERect **);
	virtual bool open(void *ptr);
	virtual void close();
	
	virtual void setParameter (long index, float value);

#if MAC
	virtual void top() {}
	virtual void sleep() {}
#endif
	void update();
	long getTag();

private:
	class CHDATA *pVoTbl;
	class OPMDRV *pOPMdrv;
	class VOPM *Effect;
	CBitmap *hBackground;
};
