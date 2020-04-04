//---------------------------------------------------------------------------
//#include "vstgui.h"
//#include "vstcontrols.h"
#include "form.h"
#include "disp.h"
#include "Slider.hpp"
#include "stdio.h"
#include "cfileselector.h"
#include "ResVOPM.h"
#include "globhead.h"
#if AU
#include "plugguieditor.h"
#else
#include "aeffguieditor.h"
#endif

#ifdef _MSC_VER
#include <tchar.h>
#endif

#if DEBUG
extern void DebugPrint (char *format, ...);
#endif
//---------------------------------------------------------------------------
int TblOpPrmMax[10]={
//TL,AR,D1R,D1L,D2R,RR,KS,MUL,DT1,DT2
	127,31,31,15,31,15,3,15,7,3
};

int TblProgMax[4]={
	7,3,7,31
};
int TblLfoMax[3]={
	255,127,127
};

#define DISP_X 200
#define DISP_Y 20
#define DISP_WIDTH 280
#define DISP_HEIGHT 100

//---------------------------------------------------------------------------

CForm::~CForm(void)
{
	removeAll();
}

CForm::CForm(const CRect &size, void *systemWindow, VSTGUIEditorInterface *editor,class OPMDRV *p,VOPM *e)
		: CFrame(size,systemWindow,editor)
{
	int x,y;

	CPoint offset(0,0);
	pOPMdrv=p;
	pVOPM=e;
	pKnobImage=new CBitmap(IDI_KNOB);
	pSliderBgImage=new CBitmap(IDI_SLIDERBG);
	pBtnImage=new CBitmap(IDI_BTN);
	pHdswImage= new CBitmap(IDI_HDSW);
	pEditor=editor;

	pKnobImage->setTransparentColor( kWhiteCColor );
	pBtnImage->setTransparentColor( kWhiteCColor );
	
	//ディスプレイ登録
#define X_CON 225
#define Y_CON 25
	CRect DispSize (DISP_X,DISP_Y,DISP_X+DISP_WIDTH,DISP_Y+DISP_HEIGHT);
	pDisp= new CDisp(DispSize,p);
	addView(pDisp);

	int i,j,index;
	//Hex-Dec表示選択ボタン登録
	x=15;
	y=42;
	pHdsw=new COnOffButton(
		CRect(x,y,x+pHdswImage->getWidth(),y+pHdswImage->getHeight()/2),
		this,
		kHdsw,
		pHdswImage
	);
	addView(pHdsw);
	//PROG SLIDを登録
	for(index=kFL,j=0;index<=kNFQ;index++,j++){
		x=20+j*42;
		y=220;
		pSlid2[j]=new CDispSlider(
			CRect(x,y,x+pSliderBgImage->getWidth(),y+pSliderBgImage->getHeight()),
			this,index,
			TblProgMax[j],
			0,
			0,
			pKnobImage,
			pSliderBgImage
		);
		addView(pSlid2[j]);
	}
	//LFO SLIDを登録
	for(index=kSPD,j=0;index<=kPMD;index++,j++){
		x=80+j*32;
		y=370;
		pSlid3[j]=new CDispSlider(
			CRect(x,y,x+pSliderBgImage->getWidth(),y+pSliderBgImage->getHeight()),
			this,index,
			TblLfoMax[j],
			0,
			0,
			pKnobImage,
			pSliderBgImage
		);
		addView(pSlid3[j]);
	}

	//WFボタンを登録
	for(index=kWf0,j=0;index<=kWf3;index++,j++){
		x=20;
		y=375+j*20;

		pWFBtn[j]=new class COnOffButton(
			CRect(x,y,x+pBtnImage->getWidth(),y+(pBtnImage->getHeight()/2)),
			this,index,pBtnImage);
		pWFBtn[j]->setTransparency (true);
		addView(pWFBtn[j]);
	}

	//CONボタンを登録
	for(index=kCon0,j=0;index<=kCon7;index++,j++){
		x=196+j*22;
		y=127;
		pConBtn[j]=new class COnOffButton(
			CRect(x,y,x+pBtnImage->getWidth(),y+(pBtnImage->getHeight()/2)),
			this,index,pBtnImage);
		pConBtn[j]->setTransparency (true);
		addView(pConBtn[j]);
	}
	//NEボタンを登録
	x=150;
	y=180;
	pNEBtn = new class COnOffButton(
			CRect(x,y,x+pBtnImage->getWidth(),y+(pBtnImage->getHeight()/2)),
			this,kNE,pBtnImage);
	pNEBtn->setTransparency(true);
	addView(pNEBtn);

	index=kM1TL;
	for(j=0;j<4;j++){
		for(i=0;i<10;i++){
			x=240+i*46;
			y=207+70*j;
			pSlid[j][i] =new CDispSlider(
				CRect(x,y,x+pSliderBgImage->getWidth(),y+pSliderBgImage->getHeight()),
				this,index,
				TblOpPrmMax[i],
				0,
				0,
				pKnobImage,
				pSliderBgImage
			);
			addView(pSlid[j][i]);
			index++;
		}

	//AMEボタンを登録
		x=j*22+84;
		y=156;
		pAMEBtn[j]=new class COnOffButton(
			CRect(x,y,x+pBtnImage->getWidth(),y+(pBtnImage->getHeight()/2)),
			this,index,pBtnImage);
		pAMEBtn[j]->setTransparency (true);
		addView(pAMEBtn[j]);
		index++;
	}

	for(index=kMskM1,j=0;index<=kMskC2;index++,j++){
		//OPMSKボタンを登録
		x=j*22+84;
		y=136;
		pMskBtn[j]=new class COnOffButton(
			CRect(x,y,x+pBtnImage->getWidth(),y+(pBtnImage->getHeight()/2)),
			this,index,pBtnImage);
		pMskBtn[j]->setTransparency (true);
		addView(pMskBtn[j]);
	}
		//Fileボタン
	for(index=kFileLoad,j=0;index<=kFileSave;index++,j++){
		x=15;
		y=10+16*j;
		pFileBtn[j]=new class COnOffButton(
			CRect(x,y,x+pBtnImage->getWidth(),y+(pBtnImage->getHeight()/2)),
			this,index,pBtnImage);
		pFileBtn[j]->setTransparency (true);
		addView(pFileBtn[j]);
	}
	//char string[16];
	CRect NumSize(148,90,168,106);	

	pProgDisp=new class CTextLabel(NumSize,"000",NULL,0);
	addView(pProgDisp);
	setDirty();
}

//---------------------------------------------------------------------------

void CForm::Load(void)
{
	int Res;

	char	buf[256];
#ifdef WIN32
//	if (((AudioEffectX *)((AEffGUIEditor *)pEditor)->getEffect())->canHostDo ("openFileSelector")!=-1){
	if (0){
		static char	InBuf[MAX_PATH];
		
		VSTGUI_SPRINTF(InBuf,"*.opm");

		struct VstFileSelect Filedata;
		VstFileType opmType ("OPM File", "OPM", "opm", "opm");
		CFileSelector OpenFile((AudioEffectX *)((AEffGUIEditor *)pEditor)->getEffect());

		memset(&Filedata, 0, sizeof(VstFileSelect));
		Filedata.command=kVstFileLoad;
		Filedata.returnPath=InBuf;
		Filedata.fileTypes=&opmType;
		Filedata.nbFileTypes=1;
		Filedata.type= kVstFileType;
		Filedata.initialPath = 0;
		Filedata.future[0] = 1;
		VSTGUI_STRCPY(Filedata.title, "Import OPM Tone data by txt");

		OpenFile.run(&Filedata);
		VSTGUI_SPRINTF(buf,"%s",Filedata.returnPath);
#else
	if (1) {
		CNewFileSelector* selector = CNewFileSelector::create (getFrame (), CNewFileSelector::kSelectFile);
		if (selector)
		{
			selector->addFileExtension (CFileExtension ("OPM File", "opm"));
			//selector->setDefaultExtension (CFileExtension ("OPM File", "opm"));
			selector->setTitle("Import OPM Tone data by txt");
			selector->runModal ();
			if ( selector->getNumSelectedFiles() > 0 ) {
				const char *url = selector->getSelectedFile(0);
				strncpy(buf, url, 255);
			}
			selector->forget ();
		}
#endif
	}else{
		#ifdef WIN32
			static TCHAR	InBuf[MAX_PATH];
		
			VSTGUI_SPRINTF(InBuf,TEXT("*.opm"));

			const TCHAR FileOpt[32]=TEXT("OPM File {*.opm}\0.opm\0\0\0");
			const TCHAR FileTitle[32]=TEXT("Import OPM Tone data by txt");	
			//VST openFileSelectorが未対応の場合,Win32APIで試す
			static OPENFILENAME ofn;
			memset(&ofn,0,sizeof(OPENFILENAME));
			ofn.lStructSize=sizeof(OPENFILENAME);
			ofn.Flags = OFN_HIDEREADONLY;
			ofn.nFilterIndex = 1;
			ofn.hwndOwner=(HWND)getSystemWindow();
			ofn.lpstrFilter =FileOpt;
			ofn.lpstrFile =InBuf;
			ofn.nMaxFile=MAX_PATH;
			ofn.lpstrFileTitle=(TCHAR*)FileTitle;
			ofn.nMaxFileTitle=MAX_PATH;
			ofn.lpstrDefExt=TEXT("opm");

			if(GetOpenFileName((LPOPENFILENAME)&ofn)){
			  InvalidateRect(ofn.hwndOwner,NULL,true);

			}else{

			}
			if(ofn.lpstrFile){
				VSTGUI_SPRINTF(buf,"%s",ofn.lpstrFile);
			}
		#else
				//機種特有のFileSelectorを呼び出し、
				//フルパスのファイル名をchar buf[]に返す
			return;
		#endif
		}
		
		Res=pOPMdrv->Load(buf);
		
		switch(Res&0xffff0000){
				
			case ERR_NOERR:
				sprintf(buf,"Load Complite.");
				break;
				
			case ERR_NOFILE:
				sprintf(buf,"Load Err can't find File.");
				break;
				
			case ERR_SYNTAX:
				sprintf(buf,"Load Err Syntax in %d",Res&0xffff);
				break;
				
			case ERR_LESPRM:
				sprintf(buf,"Load Err Less Parameter in %d",Res&0xffff);
				break;
			default :
				sprintf(buf,"Load ???");
		}
		
#ifdef DEBUG
DebugPrint(buf);
#endif
#if AU
		pVOPM->updateProgramNames();
#endif
	setDirty();
//(AudioEffectX *)pVOPM->updateDisplay();
}

void CForm::Save(void)
{
#ifdef WIN32
	struct VstFileSelect Filedata;
#endif
	int Res;
	char	buf[256];
#ifdef WIN32
//	if (((AudioEffectX *)pEditor->getEffect())->canHostDo ("openFileSelector")!=-1){
	if (0){
		char	 InBuf[256];
		
		VSTGUI_SPRINTF(InBuf,"*.opm");

		VstFileType opmType ("OPM File", "OPM", "opm", "opm");
		CFileSelector SaveFile((AudioEffectX *)((AEffGUIEditor *)pEditor)->getEffect());

		memset(&Filedata, 0, sizeof(VstFileSelect));
		Filedata.returnPath=InBuf;
		Filedata.command=kVstFileSave;
		Filedata.fileTypes=&opmType;
		Filedata.nbFileTypes=1;
		Filedata.type				= kVstFileType;
		Filedata.initialPath = 0;
		Filedata.future[0] = 1;
		VSTGUI_STRCPY(Filedata.title, "Export OPM Tone data by txt");
		SaveFile.run(&Filedata);
		VSTGUI_SPRINTF(buf,"%s",Filedata.returnPath);
#else
	if (1) {
		CNewFileSelector* selector = CNewFileSelector::create (getFrame (), CNewFileSelector::kSelectSaveFile);
		if (selector)
		{
			selector->addFileExtension (CFileExtension ("OPM File", "opm"));
			selector->setDefaultExtension (CFileExtension ("OPM File", "opm"));
			selector->setTitle("Export OPM Tone data by txt");
			selector->runModal ();
			if ( selector->getNumSelectedFiles() > 0 ) {
				const char *url = selector->getSelectedFile(0);
				strncpy(buf, url, 255);
			}
			selector->forget ();
		}
#endif
	}else{
	#ifdef WIN32
		TCHAR	 InBuf[256];
		
		VSTGUI_SPRINTF(InBuf,TEXT("*.opm"));

		const TCHAR FileOpt[32]=TEXT("OPM File {*.opm}\0.opm\0\0\0");
		const TCHAR FileTitle[32]=TEXT("Export OPM Tone data by txt");		
		//VST saveFileSelectorが未対応の場合,Win32APIで試す
		OPENFILENAME ofn;
		memset(&ofn,0,sizeof(OPENFILENAME));
		ofn.lStructSize=sizeof(OPENFILENAME);
		ofn.hwndOwner=(HWND)getSystemWindow();
		ofn.lpstrFilter =FileOpt;
		ofn.lpstrFile =InBuf;
		ofn.nMaxFile=MAX_PATH;
		ofn.lpstrFileTitle=(TCHAR*)FileTitle;
		ofn.nMaxFileTitle=MAX_PATH;
		ofn.lpstrDefExt=TEXT("opm");
		if(GetSaveFileName(&ofn)){
			InvalidateRect(ofn.hwndOwner,NULL,true);
		}
		VSTGUI_SPRINTF(buf,"%s",ofn.lpstrFile);
	#else
		//機種特有のFileSelectorを呼び出し、
		//char buf[]にフルパスのファイル名を返す。
		return;
	#endif
	}
	Res=pOPMdrv->Save(buf);

	switch(Res&0xffff0000){
		case ERR_NOERR:
			sprintf(buf,"Save Complite.");
			break;

		case ERR_NOFILE:
			sprintf(buf,"Err can't find File.");
			break;

		case ERR_SYNTAX:
			sprintf(buf,"Err Syntax in %d",Res&0xffff);
			break;

		case ERR_LESPRM:
			sprintf(buf,"Err Less Parameter in %d",Res&0xffff);
			break;
	}
#ifdef DEBUG
DebugPrint(buf);
#endif
	setDirty();
//(AudioEffectX *)pVOPM->updateDisplay();
}

void CForm::SaveS98(void)
{
	int Res;
	char	buf[256];
#ifdef WIN32
	TCHAR	 InBuf[256];
	
	VSTGUI_SPRINTF(InBuf,TEXT("*.S98"));
	
	const TCHAR FileOpt[32]=TEXT("S98 File {*.S98}\0.S98\0\0\0");
	const TCHAR FileTitle[32]=TEXT("Save S98 Log data");		
	//VST saveFileSelectorが未対応の場合,Win32APIで試す
	OPENFILENAME ofn;
	memset(&ofn,0,sizeof(OPENFILENAME));
	ofn.lStructSize=sizeof(OPENFILENAME);
	ofn.hwndOwner=(HWND)getSystemWindow();
	ofn.lpstrFilter =FileOpt;
	ofn.lpstrFile =InBuf;
	ofn.nMaxFile=MAX_PATH;
	ofn.lpstrFileTitle=(TCHAR*)FileTitle;
	ofn.nMaxFileTitle=MAX_PATH;
	ofn.lpstrDefExt=TEXT("S98");
	if(GetSaveFileName(&ofn)){
		InvalidateRect(ofn.hwndOwner,NULL,true);
	}
	VSTGUI_SPRINTF(buf,"%s",ofn.lpstrFile);
#else
	CNewFileSelector* selector = CNewFileSelector::create (getFrame (), CNewFileSelector::kSelectSaveFile);
	if (selector) {
		selector->addFileExtension (CFileExtension ("S98 File", "S98"));
		selector->setDefaultExtension (CFileExtension ("S98 File", "S98"));
		selector->setTitle("Save S98 Log data");
		selector->runModal ();
		if ( selector->getNumSelectedFiles() > 0 ) {
			const char *url = selector->getSelectedFile(0);
			strncpy(buf, url, 255);
		}
		selector->forget ();
	}
#endif
	Res=pOPMdrv->SaveS98Log(buf);
	
	switch(Res&0xffff0000){
		case ERR_NOERR:
			sprintf(buf,"Save Complite.");
			break;
			
		case ERR_NOFILE:
			sprintf(buf,"Err can't find File.");
			break;
			
		case ERR_SYNTAX:
			sprintf(buf,"Err Syntax in %d",Res&0xffff);
			break;
			
		case ERR_LESPRM:
			sprintf(buf,"Err Less Parameter in %d",Res&0xffff);
			break;
	}
#ifdef DEBUG
	DebugPrint(buf);
#endif
	setDirty();
//(AudioEffectX *)pVOPM->updateDisplay();
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

void	CForm::draw(CDrawContext *context)
{
//#if DEBUG
//	DebugPrint("exec CForm::draw");
//#endif
	if (context) CFrame::draw(context);

	//SliderRePaint
	int i,j,index;
	//CON ボタンを描く
	for(index=kCon0,i=0;index<=kCon7;index++,i++){
		pConBtn[i]->setValue(pOPMdrv->getPrm(index));
		if (context) pConBtn[i]->draw(context);
	}

	//PROG SLIDを描く
	for(index=kFL,i=0;index<=kNFQ;index++,i++){
		pSlid2[i]->setValue(pOPMdrv->getPrm(index));
		if (context) pSlid2[i]->draw(context);
	}

	//LFO SLIDを描く
	for(index=kSPD,i=0;index<=kPMD;index++,i++){
		pSlid3[i]->setValue(pOPMdrv->getPrm(index));
		if (context) pSlid3[i]->draw(context);
	}

	//WFボタンを描く
	for(index=kWf0,i=0;index<=kWf3;index++,i++){
		pWFBtn[i]->setValue(pOPMdrv->getPrm(index));
		if (context) pWFBtn[i]->draw(context);
	}


	//NEボタンを描く
	pNEBtn->setValue(pOPMdrv->getPrm(kNE));
	if (context) pNEBtn->draw(context);

	index=kM1TL;
	for(j=0;j<4;j++){
		for(i=0;i<10;i++){
			pSlid[j][i]->setValue(pOPMdrv->getPrm(index));
			if (context) pSlid[j][i]->draw(context);
			index++;
		}
		//F_AMEボタンを描く
		pAMEBtn[j]->setValue(pOPMdrv->getPrm(index));
		if (context) pAMEBtn[j]->draw(context);
		index++;
	}

	for(index=kMskM1,i=0;index<=kMskC2;index++,i++){
		//OPMSKボタンを描く
		pMskBtn[i]->setValue(pOPMdrv->getPrm(index));
		if (context) pMskBtn[i]->draw(context);
	}

	if (context) pFileBtn[0]->draw(context);
	if (context) pFileBtn[1]->draw(context);
	if (context) pDisp->EGBPaint(context);

	//アルゴリズムの図を描く
	if (context) pDisp->ConPaint(context,pOPMdrv->getPrm(kCon));

	//プログラムナンバーを書く
	char string[8];
	sprintf(string,"%03d",pOPMdrv->getProgNum()+1);
//#ifdef DEBUG
//::MessageBox(NULL,string,"VOPM",MB_OK);
//#endif

	//drawString (string,NumSize,false,kCenterText);
	pProgDisp->setFontColor (kWhiteCColor);
	pProgDisp->setFont(kNormalFont);
	pProgDisp->setText(string);
	if (context) pProgDisp->draw(context);	
}

void CForm::valueChanged (/*CDrawContext* context, */CControl* control){
	CDrawContext *context = new CDrawContext(this,NULL);
	int i,j,index;

//#if DEBUG
//	DebugPrint("exec CForm::valueChanged");
//#endif
	long tag = control->getTag ();
	
	switch (tag){
		case kFileLoad:
			Load();
			control->setValue(0);
			//if(context)control->draw(context);
			control->setDirty(true);
			break;

		case kFileSave:
		{
			long	mousekey = getCurrentMouseButtons();
			if ( mousekey & kAlt ) {
				pOPMdrv->EndS98Log();
				if ( pOPMdrv->CanSaveS98() ) {
					SaveS98();
				}
			}
			else {
				Save();
			}
			control->setValue(0);
			//if(context)control->draw(context);
			control->setDirty(true);
			break;
		}

		case kWf0: case kWf1: case kWf2: case kWf3:

			for(i=kWf0;i<=kWf3;i++){
				if(i!=tag){
					pWFBtn[i-kWf0]->setValue(0);
				}else{
					pWFBtn[i-kWf0]->setValue(1);
				}

				//if(context)pWFBtn[i-kWf0]->draw(context);
				pWFBtn[i-kWf0]->setDirty(true);
			}
			pOPMdrv->setPrm(kWF,(tag-kWf0));
			setDirty();	//	if(context)update(context);
			break;

		case kCon0: case kCon1: case kCon2: case kCon3:
		case kCon4: case kCon5: case kCon6: case kCon7:
			for(i=kCon0;i<=kCon7;i++){
				if(i!=tag){
					pConBtn[i-kCon0]->setValue(0);
				}else{
					pConBtn[i-kCon0]->setValue(1);
				}
				//if(context)pConBtn[i-kCon0]->draw(context);
			}
			pOPMdrv->setPrm(kCon,(tag-kCon0));
			//if(pDisp&&context)pDisp->ConPaint(context,pOPMdrv->getPrm(kCon));
			pDisp->setDirty(true);	//	if(context)update(context);
			break;

		case kM1TL: case kM1AR: case kM1D1R: case kM1D2R: case kM1D1L: case kM1RR:
		case kM2TL: case kM2AR: case kM2D1R: case kM2D2R: case kM2D1L: case kM2RR:
		case kC1TL: case kC1AR: case kC1D1R: case kC1D2R: case kC1D1L: case kC1RR:
		case kC2TL: case kC2AR: case kC2D1R: case kC2D2R: case kC2D1L: case kC2RR:
			pOPMdrv->setPrm(tag,(unsigned char)control->getValue());
			//if(pDisp&&context)pDisp->EGBPaint(context);
			pDisp->setDirty(true);
			break;

		case kHdsw:
		//FL等のスライダを描く
			for(index=kFL,i=0;index<=kNFQ;index++,i++){
				pSlid2[i]->setHexText(control->getValue()==1.0?true:false);
				//if(context)pSlid2[i]->draw(context);
				pSlid2[i]->setDirty(true);
			}

		//LFO SLIDを描く
			for(index=kSPD,i=0;index<=kPMD;index++,i++){
				pSlid3[i]->setHexText(control->getValue()==1.0?true:false);
				//if(context)pSlid3[i]->draw(context);
				pSlid3[i]->setDirty(true);
			}

			//オペレータパラメータのスライダを描く
			for(j=0;j<4;j++){
				for(i=0;i<10;i++){
					pSlid[j][i]->setHexText(control->getValue()==1.0?true:false);
					//if(context)pSlid[j][i]->draw(context);
					pSlid[j][i]->setDirty(true);
				}
			}
			//表示切替スイッチを描く
			//if(context)pHdsw->draw(context);
			pHdsw->setDirty(true);
			break;

			default:
				pOPMdrv->setPrm(tag,(unsigned char)control->getValue());
				setDirty();	//	if(context)update(context);
				break;
	}
	delete context;
}

long CForm::onKeyDown (VstKeyCode& keyCode)
{
	CView *pCuView;
	long result = -1;
//#if DEBUG
//	DebugPrint("exec CForm::onKeyDown");
//#endif	

	//マウスカーソルがさしているViewに対してのみディスパッチ
	if(pCuView=pMouseOverView){
		if((result=pCuView->onKeyDown(keyCode))!=-1){
			return(-1);
		}
	}
	return result;
}
void CForm::setFocusView (CView *pView)
{
	int Tag;
#if DEBUG
	if(pFocusView){
		Tag=((CControl *)pFocusView)->getTag();		
		DebugPrint("CForm::setFocusView pFocusView Tag=%d",Tag);	
	}else{
		DebugPrint("CForm::setFocusView pFocusView is NULL");
	}
#endif
	if (pFocusView){
		//前回のFocusViewのindexを取得
		Tag=((CControl *)pFocusView)->getTag();
		if(
			(Tag>=kFL && Tag<=kNFQ)||
			(Tag>=kSPD && Tag<=kPMD)||
			(Tag>=kM1TL && Tag<=kM1DT2)||
			(Tag>=kC1TL && Tag<=kC1DT2)||
			(Tag>=kM2TL && Tag<=kM2DT2)||
			(Tag>=kC2TL && Tag<=kC2DT2)
		){
			((CDispSlider *)pFocusView)->setLearn(false);
		}
	}
	CFrame::setFocusView(pView);
}
