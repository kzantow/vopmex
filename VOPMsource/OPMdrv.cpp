//OPMdrv.cpp
//Midi->OPM パラメータ、音色管理
//2002.3.11 sam
//based MiOPMdrv

#ifdef _WIN32
#include "Gmcc86.h"
#else
#include "GmcMB2.h"
#endif
#include "OPMdrv.hpp"
#include "OPM.hpp"
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "VOPM.hpp"

#include "globhead.h"

#if DEBUG
extern void DebugPrint (char *format, ...);
#endif

//Class LFODATA
//LFO Data Class
const int Ktbl[12]={0,1,2,4,5,6,8,9,10,12,13,14};

const int is_vol_set[8][4]={
	{0,0,0,1},
	{0,0,0,1},
	{0,0,0,1},
	{0,0,0,1},
	{0,0,1,1},
	{0,1,1,1},
	{0,1,1,1},
	{1,1,1,1}
};

//#define _iocs_opmset(RegNo,data) pOPM->OpmPoke(RegNo,data,SampleTime)
inline void OPMDRV::_iocs_opmset( unsigned char RegNo,unsigned char data )
{
	//レジスタをログへ
	if ( mIsDumperRunning ) {
		m_pRegDumper->DumpReg(RegNo, data, static_cast<int>(((mFrameStartPos+SampleTime) * mTickPerSec) / Samprate + 0.5) );
	}
	//GIMICにレジスタ情報を送信
	if ( m_pRealChip ) {
		m_pRealChip->Out(RegNo, data, (SampleTime * 1000) / Samprate );
	}
	//エミュレーション音源に送信
	pOPM->OpmPoke(RegNo,data,SampleTime);
}

//Class OPDATA
//Operater Data Class

OPDATA::OPDATA(void){
	//EG-Level
	TL=0x00;
	D1L=0x0;
	//EG-ADR
	AR=0x1f;
	D1R=0x00;
	D2R=0x00;
	RR=0x4;
	//DeTune
	KS=0;
	DT1=0;
	MUL=1;
	DT2=0;
	F_AME=0;
}

//Class CHDATA
//chData（音色） クラス
CHDATA::CHDATA(void){
	F_PAN=0x40;
	CON=0;
	FL=0;
	AMS=0;
	PMS=0;
	OpMsk=0x40;
	F_NE=0;
	SPD=0;
	PMD=0;
	AMD=0;
	WF=0;
	NFQ=0;
	strcpy(Name,"init");
}


//Class OPMDRV
//Midi to OPM Emu Driver
void OPMDRV::setDelta(unsigned int Delta){
	SampleTime=Delta;
}

void OPMDRV::AutoSeq(int sampleframes){
//ポルタメント&LFO用
//AutoSeqはOPMの波形レンダリング処理からコールバックされる。
//次回のレンダリング開始時に、
//この関数が発行したコマンドをすべて実行させる。
	int i;
	int Porta;
	int	slotFrqLfo[CHMAX];
	int	slotFrqEnv[CHMAX];
	int slotSpdConst[CHMAX];
	for(i=0;i<CHMAX;i++){
		slotFrqLfo[i] = 0;
		slotFrqEnv[i] = 0;
		slotSpdConst[i] = -1;
	}
	
	setDelta(0);
	
	//発音順に処理するためのchテーブルを作成
	int	chList[CHMAX];
	if ( PolyMono ) {
		list<int>::iterator	it;
		i=0;
		if ( WaitCh.size() > 0 ) {
			it = WaitCh.begin();
			for (; (i<CHMAX) && (it != WaitCh.end()); i++) {
				chList[i] = *it;
				it++;
			}
		}
		if ( PlayCh.size() > 0 ) {
			it = PlayCh.begin();
			for (; (i<CHMAX) && (it != PlayCh.end()); i++) {
				chList[i] = *it;
				it++;
			}
		}
	}
	else {
		for (i=0; i<CHMAX; i++) {
			chList[i] = i;
		}
	}
	
	for(i=0;i<CHMAX;i++){
		int	ch = chList[i];
		int MidiCh = TblCh[ch].MidiCh;
		
		//ポルタメント処理
		Porta=TblCh[ch].PortaBend;
		if(Porta!=0){
			if(Porta<-1023){
				Porta+=TblCh[ch].PortaSpeed;
			}else{
				if(Porta>+1023){
					Porta-=TblCh[ch].PortaSpeed;
				}else{Porta=0;}
			}

			TblCh[ch].PortaBend=Porta;
			//SendKc(ch);
		}
		
		//ソフトLFO処理
		for ( int j=0; j<SWLFO_NUM; j++ ) {
			//ソフトウェアLFOオン処理
			int delayCnt = TblCh[ch].SwLfoDelayCnt[j];
			if ( delayCnt > 0 ) {
				if ( delayCnt == 1 ) {
					TblCh[ch].SwLfo[j].SetPMSAMS(ch, (TblMidiCh[MidiCh].SwLfo[j].PMS<<4) | TblMidiCh[MidiCh].SwLfo[j].AMS);
					if ( TblMidiCh[MidiCh].SwLfo[j].Sync ) {
						TblCh[ch].SwLfo[j].LfoReset();
						TblCh[ch].SwLfo[j].LfoStart();
					}
				}
				delayCnt--;
			}
			TblCh[ch].SwLfoDelayCnt[j] = delayCnt;
			
			TblCh[ch].SwLfo[j].Update();
		}
		
		//ソフトエンベロープ処理
		for ( int j=0; j<SWENV_NUM; j++ ) {
			TblCh[ch].Env[j].Update();
		}
		
		//コンプレッサー
		{
			float reduction = pOPM->GetOutAmp(TblCh[ch].MidiCh);
			if ( reduction > .0f ) {
				TblCh[ch].reduction = CalcTLFromAmp( reduction * reduction * TblMidiCh[TblCh[ch].MidiCh].CompOutGain );
			}
			else {
				//負数はオフ状態を表す
				TblCh[ch].reduction = .0f;
			}
		}
		
		//音量ピッチ更新処理
		if ( TblCh[ch].Vel > 0 ) {
			CHDATA ChData;
			mergeControlChange(&ChData, TblCh[ch].VoNum, MidiCh);
			setTL(ch,CalcLimTL(ChData.Op[0].TL,TblMidiCh[MidiCh].OpTL[0]),0);
			setTL(ch,CalcLimTL(ChData.Op[1].TL,TblMidiCh[MidiCh].OpTL[1]),1);
			setTL(ch,CalcLimTL(ChData.Op[2].TL,TblMidiCh[MidiCh].OpTL[2]),2);
			setTL(ch,CalcLimTL(ChData.Op[3].TL,TblMidiCh[MidiCh].OpTL[3]),3);

			SendKc(ch);
			
			if ( MidiCh == 0 ) {
				//実機OPMの場合は、すべて1スロット目に合算する
				int envCh = IsRealMode() ? 0:ch;
				slotSpdConst[envCh] = TblMidiCh[MidiCh].SPD + (ChData.SPD << 7);	//CCによる設定値(8bit->15bit)
				
				//ソフトLFO->LFOFRQ更新処理
				//一番後発のslotの、MIDIChの全てのLFO値のOR
				bool	isFirst=true;
				for (int j=0; j<SWLFO_NUM; j++) {
					if ( TblMidiCh[MidiCh].SwLfo[j].Dest & SWENV_DEST_LFOFRQ ) {
						int	lfoValue = TblCh[ch].SwLfo[j].GetAmValue(ch) << 7;
						if ( lfoValue > slotFrqLfo[envCh] || isFirst ) {
							slotFrqLfo[envCh] = lfoValue;			//発音順に処理しているので、後発で上書きされる
						}
						isFirst = false;
					}
				}
				
				//ソフトエンベロープ->LFOFRQ更新処理
				//slot発音の全エンベロープ値のOR
				for (int j=0; j<SWENV_NUM; j++) {
					if ( TblMidiCh[MidiCh].SwEnv[j].Dest & SWENV_DEST_LFOFRQ ) {
						int envValue = TblCh[ch].Env[j].GetEnvValue() * TblMidiCh[MidiCh].SwEnv[j].Depth;
						if ( envValue > slotFrqEnv[envCh] ) {
							slotFrqEnv[envCh] = envValue;
						}
					}
				}
			}
		}
		
		//オートパン処理
		if ( TblMidiCh[TblCh[ch].MidiCh].AutoPan.Enable ) {
			if ( TblCh[ch].AutoPanDelayCnt > 0 ) {
				if ( TblCh[ch].AutoPanDelayCnt == 1 ) {
					TblCh[ch].AutoPanEnable = true;
				}
				TblCh[ch].AutoPanDelayCnt--;
			}
			if ( TblCh[ch].AutoPanEnable ) {
				int	count = static_cast<int>(mBeatPos / TblMidiCh[TblCh[ch].MidiCh].AutoPan.StepTime);
				count %= TblMidiCh[TblCh[ch].MidiCh].AutoPan.Length;
				int pan = TblMidiCh[TblCh[ch].MidiCh].AutoPan.Table[count];
				SendPan(ch, pan);
			}
		}
		else {
			SendPan(ch, TblMidiCh[TblCh[ch].MidiCh].Pan);
		}
		
		//LFOオン処理
		int Cnt=TblCh[ch].LFOdelayCnt;
		if(Cnt!=0){
			if(Cnt==1){
				CHDATA	ChData;
				mergeControlChange(&ChData, TblCh[ch].VoNum, TblCh[ch].MidiCh);
				_iocs_opmset(0x00,ch);
				if ( TblMidiCh[TblCh[ch].MidiCh].LFOSync ) _iocs_opmset(0x01,0x02);//LfoReset
				_iocs_opmset(0x38+ch,(ChData.PMS<<4)|(ChData.AMS&0x3));//PMS/AMS
				if ( TblMidiCh[TblCh[ch].MidiCh].LFOSync ) _iocs_opmset(0x01,0x00);//LfoStart
			}
			Cnt--;
		}
		TblCh[ch].LFOdelayCnt=Cnt;
	}
	
	//LFOFRQ更新実処理
	for(i=0;i<CHMAX;i++){
		if ( slotSpdConst[i] != -1 ) {
			int frqsum = CalcLim8( (slotFrqLfo[i] + slotFrqEnv[i])>>9, slotSpdConst[i] );
			//printf("%d frqsum=%d,\n",i,frqsum);
			setSPD(i,frqsum);
		}
	}
}


void OPMDRV::setProgNum(long ProgNum){
	CuProgNum=ProgNum;
#ifdef DEBUG
	DebugPrint("OPMdrv::setProgNum %d",ProgNum);
#endif
	
#if 0
	for(int i=0;i<16;i++){
		TblMidiCh[i].VoNum=ProgNum;
		ResetAllProgramParam(i);
	}
#else
	TblMidiCh[0].VoNum=ProgNum;
	ResetAllProgramParam(0);
#endif

}
//get/setChunk用のプログラムチェンジしない版
void OPMDRV::setCuProgNum(long ProgNum){
	CuProgNum=ProgNum;
}
int OPMDRV::getProgNum(void){
	return((int)CuProgNum);
}
void OPMDRV::setProgName(char *Name){
	strcpy(VoTbl[CuProgNum].Name,Name);
	strcpy(CuProgName,Name);
}

char* OPMDRV::getProgName(void){
	strcpy(CuProgName,VoTbl[CuProgNum].Name);
	return(CuProgName);
}
char* OPMDRV::getProgName(int index){
	return(VoTbl[index].Name);
}

void OPMDRV::setRPNH(int ch,int data){
	TblMidiCh[ch].RPN&=0x007f;
	TblMidiCh[ch].RPN|=(data&0x007f)<<8;
	TblMidiCh[ch].CuRPN=true;

}
void OPMDRV::setRPNL(int ch,int data){
	TblMidiCh[ch].RPN&=0x7f00;
	TblMidiCh[ch].RPN|=data&0x007f;
	TblMidiCh[ch].CuRPN=true;
}
void OPMDRV::setNRPNH(int ch,int data){
	TblMidiCh[ch].NRPN&=0x007f;
	TblMidiCh[ch].NRPN|=(data&0x007f)<<8;
	TblMidiCh[ch].CuRPN=false;
}
void OPMDRV::setNRPNL(int ch,int data){
	TblMidiCh[ch].NRPN&=0x7f00;
	TblMidiCh[ch].NRPN|=data&0x007f;
	TblMidiCh[ch].CuRPN=false;
}

void OPMDRV::setData(int ch,int data){
	if(TblMidiCh[ch].CuRPN==true){
	//RPN Data Entr
		switch(TblMidiCh[ch].RPN){
			case 0x0000://ピッチベンドセンス
				if(data<=0 || data>=96){data=2;}
				TblMidiCh[ch].BendRang=data;
				break;
		}
	}else{
	//NRPN Data Entr
		switch(TblMidiCh[ch].NRPN){
			case 0x0000://OPMクロック設定
				if(data<=0x3f){
					//OPM Clock=3.58MHz:default
					pOPM->SetOpmClock(3580000);
					if ( m_pRealChip ) {
						m_pRealChip->SetPLLClock(3580000);	//GIMICの動作クロックを変更
					}
					OpmClock=0;
				}else{
					if(data<=0x5f){
						//OPM Clock=3.579545MHz
						pOPM->SetOpmClock(3579545);
						if ( m_pRealChip ) {
							m_pRealChip->SetPLLClock(3579545);	//GIMICの動作クロックを変更
						}
						OpmClock=2;
					}else{
						//OPM Clock=4MHz
						pOPM->SetOpmClock(4000000);
						if ( m_pRealChip ) {
							m_pRealChip->SetPLLClock(4000000);	//GIMICの動作クロックを変更
						}
						OpmClock=1;
					}
				}
				break;
			case 0x0001://音量バイアス設定
				_iocs_opmset(0x5,data);
				break;
			case 0x0002://LPF ON/OFF設定
				if(data<=0x3f){
					//ON:default
					_iocs_opmset(0x6,0);
				}else{
					//OFF
					_iocs_opmset(0x6,1);
				}
				break;
			case 0x7f00://コントロールチェンジモードチャンネル個別切り替え
				if ( data == 0 ) {
					TblMidiCh[ch].CcMode = CCMODE_ORIGINAL;
				}
				else {
					TblMidiCh[ch].CcMode = CCMODE_EXPAND;
				}
				break;
			case 0x7f7f://コントロールチェンジモード全チャンネル一括切り替え
				for ( int i=0; i<16; i++ ) {
					if ( data == 0 ) {
						TblMidiCh[i].CcMode = CCMODE_ORIGINAL;
					}
					else {
						TblMidiCh[i].CcMode = CCMODE_EXPAND;
					}
				}
				break;
			case 0x7e00://コントロールチェンジ方向チャンネル個別切り替え
				if ( data == 0 ) {
					TblMidiCh[ch].CcDirection = CCDIRECTION_NATURAL;
				}
				else {
					TblMidiCh[ch].CcDirection = CCDIRECTION_REGISTER;
				}
				break;
			case 0x7e7f://コントロールチェンジ方向全チャンネル一括切り替え
				for ( int i=0; i<16; i++ ) {
					if ( data == 0 ) {
						TblMidiCh[i].CcDirection = CCDIRECTION_NATURAL;
					}
					else {
						TblMidiCh[i].CcDirection = CCDIRECTION_REGISTER;
					}
				}
				break;
		}
	}
}
	
void OPMDRV::SendKc(int ch){
	unsigned int Word,Note;
	unsigned char kc,kf;
	unsigned char MidiCh;
	MidiCh=TblCh[ch].MidiCh;
	Note=TblCh[ch].Note;
	//キーオフされて0xffになっていたら、コピーを使う
	if(Note==0xff) {
		Note=TblCh[ch].CopyOfNote;
	}
	if(Note<0x80){
		Word=(Note<<16)+((TblMidiCh[MidiCh].Bend<<3)*TblMidiCh[MidiCh].BendRang)+TblCh[ch].PortaBend;	//16.16bit固定小数点
		for ( int i=0; i<SWLFO_NUM; i++ ) {
			if ( TblMidiCh[MidiCh].SwLfo[i].Dest & SWENV_DEST_PITCH ) {
				Word+=TblCh[ch].SwLfo[i].GetPmValue(ch)<<10;	//ソフトウェアLFOを反映
			}
		}
		for ( int i=0; i<SWENV_NUM; i++ ) {
			if ( TblMidiCh[MidiCh].SwEnv[i].Dest & SWENV_DEST_PITCH ) {
				Word+=(TblCh[ch].Env[i].GetEnvValue() * TblMidiCh[MidiCh].SwEnv[i].Depth) << 5;
			}
		}
		
		switch(OpmClock){
			case 1://4MHz
				//OPM Clockが4MHzの場合、周波数を補正 12*log2(4MHz/3.58MHz)*65536<16.16bit固定小数点のoffset>
				Word-=125861;
				break;
			case 2://3.579545MHzの場合、周波数を補正 12*log2(3.579545MHz/3.58MHz)*65536<16.16bit固定小数点のoffset>
				Word+=144;
				break;
		}
		
		Note=Word>>16;
		//	kc=(Ktbl[(Note-3)%12])+((int)((Note-3)/12)*16); //Kc[Oct::key]の算出
		
		kc=(Ktbl[(Note-1)%12])+((int)((Note-1)/12)*16); //Kc[Oct::key]の算出
		kf=(Word>>8)&0xfc;							 //Kf[ベンド微調節]の算出
		_iocs_opmset(0x28+ch,kc);	// 音階をOPMレジスタに書き込む
		_iocs_opmset(0x30+ch,kf);	//	ベンドをOPMレジスタに書き込む
	}
#ifdef MyDEBUG
	fprintf(stdout,"kc:%02x kf:%02x\n",kc,kf);
#endif
}

void OPMDRV::SendPan(int ch,int F_PAN){
	ch&=0x07;
	CHDATA	ChData;
	mergeControlChange(&ChData, TblCh[ch].VoNum, TblCh[ch].MidiCh );	
	unsigned char tmp;
	tmp=(ChData.CON&0x07)|((ChData.FL&0x7)<<3);
	if ( F_PAN < 32 ) {
		tmp |= 0x40;
	}
	else if ( F_PAN > 96 ) {
		tmp |= 0x80;
	}
	else {
		tmp |= 0xc0;
	}
	_iocs_opmset(0x20+ch,tmp);
	_iocs_opmset(0x00,ch);
	_iocs_opmset(0x03,F_PAN);
}

void OPMDRV::SendVo(int ch){
	ch&=0x07;
	int	MidiCh = TblCh[ch].MidiCh;
	CHDATA	ChData;
	mergeControlChange(&ChData, TblCh[ch].VoNum, MidiCh);
	class OPDATA *pOp;
	
	//_iocs_opmset(0x38+ch,(pCh->PMS<<4)|(pCh->AMS&0x3));//PMS/AMS
	int tmp;
	for(int i=0;i<4;i++){
		tmp=(i<<3)+ch;
		pOp=&(ChData.Op[i]);
		_iocs_opmset(0x40+tmp,(pOp->DT1<<4)|(pOp->MUL&0x0f));	//DT1/MUL
		_iocs_opmset(0x80+tmp,(pOp->KS<<6)|(pOp->AR&0x1f));	 //KS/AR
		_iocs_opmset(0xA0+tmp,(pOp->F_AME)|(pOp->D1R&0x1f)); //AME/D1R
		_iocs_opmset(0xC0+tmp,(pOp->DT2<<6)|(pOp->D2R&0x1f));	 //DT2/D2R
		_iocs_opmset(0xE0+tmp,(pOp->D1L<<4)|(pOp->RR&0x0f));	//D1L/RR
	}
}

void OPMDRV::SendLfo(int ch){
	if ( IsRealMode() == false || TblCh[ch].MidiCh == 0 ) {
		class CHDATA ChData;
		mergeControlChange( &ChData, TblCh[ch].VoNum, TblCh[ch].MidiCh );
		_iocs_opmset(0x00,ch);
		_iocs_opmset(0x1B,ChData.WF);
	}
}

OPMDRV::OPMDRV(class Opm *pTmp)
: m_pRealChip(NULL)
{
	int i;
	PolyMono=true;
	pOPM=pTmp;
	//SLOTテーブル初期化
	for(i=0;i<CHMAX;i++){
		TblCh[i].VoNum=0;
		TblCh[i].MidiCh=i;
		TblCh[i].Note=0xff;	 //Note:0xff 空きチャンネル
		TblCh[i].CopyOfNote=0xff;
		TblCh[i].Vel=0x00;	//消音
		TblCh[i].PortaBend=0;
		TblCh[i].PortaSpeed=0;
		TblCh[i].LFOdelayCnt=0;
		
		//ソフトウェアLFO初期化
		int	sampleRate = Samprate;
		Samprate = 100;	//Updateのサイクル
		for (int j=0; j<SWLFO_NUM; j++) {
			TblCh[i].SwLfo[j].Init();
			TblCh[i].SwLfoDelayCnt[j]=0;
		}
		Samprate = sampleRate;
		
		//コンプ
		TblCh[i].reduction = .0;
	}
	
	for(i=0;i<CHMAX;i++){
		WaitCh.push_back(i);
	}
	
	//ボリューム計算用
	log_2_inv = 1.0f / log10f(2.0f);
	
	mAlternatePolyMode = true;	//初期状態では従来通りの確保法
	SetLocalOn(false);
	
	for(i=0;i<16;i++){
		TblMidiCh[i].VoNum=0;
		TblMidiCh[i].Bend=0;
		ResetAllControllers(i);
		TblMidiCh[i].CcMode = CCMODE_EXPAND;
	}
	CuProgNum=0;
	strcpy(CuProgName,"");
	SampleTime=0;
	OpmClock=0;
	
	//S98ログ
	m_pRegDumper = new S98File();
	mIsDumperRunning = false;
	
	//演奏位置情報
	mTempo = 125.0;
	mBeatPos = .0;
}

void OPMDRV::OpenDevices()
{
	//GIMIC開始
	//Windowsの場合c86を使う
#ifdef _WIN32
	m_pRealChip = new Gmcc86();
	m_pRealChip->Init();
	bool	available = true;
	if (m_pRealChip->AvailableChips() == 0) {
		available = false;
	}
	else {
		// モジュールの確認
		// FD 91 XX          : get HW report, XX=番号(00=モジュール, FF=マザーボード)
		Devinfo info;
		m_pRealChip->GetModuleInfo(&info);
		if (strncmp( info.Devname, "GMC-OPM", 7)) {
			//printf("OPM Module not foundn");
			available = false;
		}
	}
	if (!available) {
		delete m_pRealChip;
		m_pRealChip = NULL;
	}
	else {
		m_pRealChip->Reset();
		m_pRealChip->SetPLLClock(3580000);	//VOPM起動時の設定
		SetLocalOn(false);
	}
#else
	//Macの場合は自前で接続する
	if (m_pRealChip == NULL) {
		m_pRealChip = new GmcMB2(this);
		m_pRealChip->Init();
		m_pRealChip->SetPLLClock(3580000);
	}
#endif
}

void OPMDRV::CloseDevices()
{
	//GIMICインスタンスを破棄
	if ( m_pRealChip ) {
		delete m_pRealChip;
	}
}

OPMDRV::~OPMDRV(void){
	delete m_pRegDumper;
}
void OPMDRV::ChDelay(int delay){
	int i;
	for (i=0;i<16;i++){
	 NoteAllOff(i);
	}
}


void OPMDRV::NoteOn(int MidiCh,int Note,int Vel){
	int ch,BfNote,PortaDelta;
	
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	
	//実機OPMの場合、ノイズONのとき強制的にSlot7で発音する
	bool	IsHardwareNoise = (ChData.F_NE!=0 && IsRealMode())? true:false;

	if(PolyMono==true){
		if ( IsHardwareNoise ) {
			if((ch=NonCh(MidiCh,Note,Vel,7))==-1){return;}
		}
		else {
			if((ch=NonCh(MidiCh,Note,Vel,-1))==-1){return;}
		}
	}else{
		MidiCh&=0x07;
		if ( IsHardwareNoise ) {
			ch = 7;	//ノイズチャンネルに固定
		}
		else {
			ch = MidiCh;
		}
		BfNote=TblCh[ch].Note;
		TblCh[ch].Note=Note;
		TblCh[ch].CopyOfNote=Note;
		TblCh[ch].Vel=Vel;
		TblCh[ch].MidiCh=MidiCh;
		TblCh[ch].VoNum=TblMidiCh[MidiCh].VoNum;
	}

	TblCh[ch].LFOdelayCnt=TblMidiCh[MidiCh].LFOdelay;
	if(TblMidiCh[MidiCh].PortaSpeed ){
		if((TblMidiCh[MidiCh].PortaOnOff==true)||
		   (PolyMono==false && BfNote<=0x7f))
		{
			PortaDelta=(TblMidiCh[MidiCh].BfNote-Note);
			TblCh[ch].PortaBend=PortaDelta*0x00010000;
			
			if(PortaDelta<0){PortaDelta*=-1;}
			TblCh[ch].PortaSpeed=(PortaDelta*0x10000)/TblMidiCh[MidiCh].PortaSpeed;
			
		}
		else{
			TblCh[ch].PortaBend=0;
			TblCh[ch].PortaSpeed=0;
		}
	}
	else{
		TblCh[ch].PortaBend=0;
		TblCh[ch].PortaSpeed=0;
	}

	//ソフトウェアLFOの設定
	for ( int i=0; i<SWLFO_NUM; i++ ) {
		TblCh[ch].SwLfoDelayCnt[i] = TblMidiCh[MidiCh].SwLfo[i].Delay;
		if ( TblCh[ch].SwLfoDelayCnt[i] == 0 ) {
			TblCh[ch].SwLfo[i].SetPMSAMS(ch, (TblMidiCh[MidiCh].SwLfo[i].PMS<<4) | TblMidiCh[MidiCh].SwLfo[i].AMS);
		}
		else {
			TblCh[ch].SwLfo[i].SetPMSAMS(ch, 0);	//ここでは無効にしておき、Autoseq内でオンにする
		}
		TblCh[ch].SwLfo[i].SetWaveForm(TblMidiCh[MidiCh].SwLfo[i].WF);
		TblCh[ch].SwLfo[i].SetLFRQ(TblMidiCh[MidiCh].SwLfo[i].FRQ<<1);
		TblCh[ch].SwLfo[i].SetPMDAMD(0x80|TblMidiCh[MidiCh].SwLfo[i].PMD);
		TblCh[ch].SwLfo[i].SetPMDAMD(TblMidiCh[MidiCh].SwLfo[i].AMD);
		if ( TblMidiCh[MidiCh].SwLfo[i].Sync ) {
			TblCh[ch].SwLfo[i].LfoReset();
			TblCh[ch].SwLfo[i].LfoStart();
		}
	}
	
	//ソフトウェアエンベロープ
	for ( int i=0; i<SWENV_NUM; i++ ) {
		TblCh[ch].Env[i].SetLoopNum(TblMidiCh[MidiCh].SwEnv[i].LoopNum);
		for ( int j=0; j<SWENV_STAGES; j++ ) {
			setSwEnvValue(ch,TblMidiCh[MidiCh].SwEnv[i].Value[j],j,i);
			setSwEnvTime(ch,TblMidiCh[MidiCh].SwEnv[i].Time[j],j,i);
		}
		TblCh[ch].Env[i].KeyOn();
	}
	
	//コンプ（Autoseqでやってる処理と同じ）
	{
		float reduction = pOPM->GetOutAmp(MidiCh);
		if ( reduction > .0f ) {
			TblCh[ch].reduction = CalcTLFromAmp( reduction * reduction * TblMidiCh[MidiCh].CompOutGain );
		}
		else {
			//負数はオフ状態を表す
			TblCh[ch].reduction = .0f;
		}
	}
	
	//オートパン
	if ( TblMidiCh[MidiCh].AutoPan.Enable ) {
		if ( TblMidiCh[MidiCh].AutoPan.Delay > 0 ) {
			TblCh[ch].AutoPanEnable = false;
			TblCh[ch].AutoPanDelayCnt = TblMidiCh[MidiCh].AutoPan.Delay;
		}
		else {
			TblCh[ch].AutoPanEnable = true;
			TblCh[ch].AutoPanDelayCnt = 0;
		}
	}
	else {
		TblCh[ch].AutoPanEnable = false;
	}
	
	//OPM拡張仕様
	_iocs_opmset(0x00,ch);
	_iocs_opmset(0x0F,(ChData.NFQ&0x1f)|(ChData.F_NE&0x80));
	
	SendLfo(ch);
	setPMD(ch,CalcLim7(ChData.PMD,TblMidiCh[MidiCh].PMD));
	setAMD(ch,CalcLim7(ChData.AMD,TblMidiCh[MidiCh].AMD));
	int frqsum = 0;
	for ( int i=0; i<SWENV_NUM; i++ ) {
		int envValue = TblCh[ch].Env[i].GetEnvValue() * TblMidiCh[MidiCh].SwEnv[i].Depth;
		if ( envValue > frqsum ) {
			frqsum = envValue;
		}
	}
	frqsum >>= 9;
	frqsum += ChData.SPD;
	frqsum = CalcLim8( frqsum, TblMidiCh[MidiCh].SPD );
	setSPD(ch,frqsum);

	SendVo(ch);
	if(TblMidiCh[MidiCh].LFOdelay==0){
		if ( TblMidiCh[MidiCh].LFOSync ) _iocs_opmset(0x01,0x02);//LfoReset
		_iocs_opmset(0x38+ch,(ChData.PMS<<4)|(ChData.AMS&0x3));//PMS/AMS
		if ( TblMidiCh[MidiCh].LFOSync ) _iocs_opmset(0x01,0x00);//LfoStart 
	}else{
		_iocs_opmset(0x38+ch,0);//PMS/AMS LFOを掛けない
	}
	
	SendPan(ch,TblMidiCh[MidiCh].Pan);
	SendKc(ch);

//TLSend
	setTL(ch,CalcLimTL(ChData.Op[0].TL,TblMidiCh[MidiCh].OpTL[0]),0);
	setTL(ch,CalcLimTL(ChData.Op[1].TL,TblMidiCh[MidiCh].OpTL[1]),1);
	setTL(ch,CalcLimTL(ChData.Op[2].TL,TblMidiCh[MidiCh].OpTL[2]),2);
	setTL(ch,CalcLimTL(ChData.Op[3].TL,TblMidiCh[MidiCh].OpTL[3]),3);

//Vel&Vol Set
	if ( (!IsRealMode()) && (!mIsDumperRunning) ) {
		_iocs_opmset(0x02,TblMidiCh[MidiCh].Vol );			//Vol拡張セット
		_iocs_opmset(0x04,TblMidiCh[MidiCh].Vol2);			//Vel拡張セット
	}
	
	//_iocs_opmset(0x01, 2);	// LFOをリセット
	if(PolyMono==false && BfNote!=0xff){
		//Monoモードで発音されてる場合、アタックなし（レガート）
	}
	else{
		if ( TblMidiCh[MidiCh].LFOSync ) _iocs_opmset(0x01, 0);	// LFOをスタート
		_iocs_opmset(0x08,(ch&0x07)+(VoTbl[TblMidiCh[MidiCh].VoNum].OpMsk&0x78));	// キーオン
	}
	
	//出力MIDIチャンネルを設定
	pOPM->SetOutMidiCh(ch, MidiCh );
	
	TblMidiCh[MidiCh].BfNote=Note;
	return;
}

void OPMDRV::NoteAllOff(int MidiCh){
	int ch;
	
	while((ch=ChkSonCh(MidiCh,0xff))!=-1){
		_iocs_opmset(0x08, ch);	// キーオフ
		DelCh(ch);
	}
}
void OPMDRV::ForceAllOff(void){
	int ch,MidiCh;
	for(MidiCh=0;MidiCh<16;MidiCh++){
		while((ch=ChkSonCh(MidiCh,0xff))!=-1){
			DelCh(ch);
		}
	}
	pOPM->ForceNoteOff();
}
void OPMDRV::ForceAllOff(int MidiCh){
	int ch;
	while((ch=ChkSonCh(MidiCh,0xff))!=-1){
		_iocs_opmset(0x09, ch);	// キーオフ
		DelCh(ch);
	}
}

void OPMDRV::NoteOff(int MidiCh,int Note){
	int ch;
	
	if(PolyMono==false){
		MidiCh&=0x07;
		ch=MidiCh;
		if(TblCh[ch].Note==Note){
			for ( int i=0; i<SWENV_NUM; i++ ) {
				TblCh[ch].Env[i].KeyOff();
			}
			_iocs_opmset(0x08,ch);
			TblCh[ch].Note=0xff;
		}
	}
	else{
		while((ch=ChkSonCh(MidiCh,Note))!=-1){
			for ( int i=0; i<SWENV_NUM; i++ ) {
				TblCh[ch].Env[i].KeyOff();
			}
			_iocs_opmset(0x08, ch);	// キーオフ
			DelCh(ch);
		}
	}
}

void OPMDRV::BendCng(int MidiCh,int Bend){
	int i;
	
	TblMidiCh[MidiCh].Bend=Bend-0x2000;
	for(i=0;i<CHMAX;i++){
		if(TblCh[i].MidiCh==MidiCh /*&& TblCh[i].Note!=0xff*/){
			SendKc(i);
		}
	}
}

void OPMDRV::VolCng(int MidiCh,int Vol){
	int i;
	
	TblMidiCh[MidiCh].Vol=Vol;
	
	for(i=0;i<CHMAX;i++){
		if(TblCh[i].MidiCh==MidiCh){
			if ( IsRealMode() || mIsDumperRunning ) {
				//実機OPMがある場合はTLの変更で対応
				CHDATA ChData;
				mergeControlChange(&ChData, TblCh[i].VoNum, MidiCh);
				setTL(i,CalcLimTL(ChData.Op[0].TL,TblMidiCh[MidiCh].OpTL[0]),0);
				setTL(i,CalcLimTL(ChData.Op[1].TL,TblMidiCh[MidiCh].OpTL[1]),1);
				setTL(i,CalcLimTL(ChData.Op[2].TL,TblMidiCh[MidiCh].OpTL[2]),2);
				setTL(i,CalcLimTL(ChData.Op[3].TL,TblMidiCh[MidiCh].OpTL[3]),3);
			}
			else {
				_iocs_opmset(0x00,i);			//チャンネル拡張セット
				_iocs_opmset(0x02,Vol);			//Vol拡張セット
			}
		}
	}
}

void OPMDRV::Vol2Cng(int MidiCh,int Vol){
	int i;
	
	TblMidiCh[MidiCh].Vol2=Vol;
	
	for(i=0;i<CHMAX;i++){
		if(TblCh[i].MidiCh==MidiCh){
			if ( IsRealMode() || mIsDumperRunning ) {
				//実機ハードウェアがある場合はTLの変更で対応
				CHDATA ChData;
				mergeControlChange(&ChData, TblCh[i].VoNum, MidiCh);
				setTL(i,CalcLimTL(ChData.Op[0].TL,TblMidiCh[MidiCh].OpTL[0]),0);
				setTL(i,CalcLimTL(ChData.Op[1].TL,TblMidiCh[MidiCh].OpTL[1]),1);
				setTL(i,CalcLimTL(ChData.Op[2].TL,TblMidiCh[MidiCh].OpTL[2]),2);
				setTL(i,CalcLimTL(ChData.Op[3].TL,TblMidiCh[MidiCh].OpTL[3]),3);
			}
			else {
				_iocs_opmset(0x00,i);			//チャンネル拡張セット
				_iocs_opmset(0x04,Vol);			//Vel拡張セット
			}
		}
	}
}

void OPMDRV::VoCng(int MidiCh,int Vo){
//	int i;
//	int Spd;
//	int Amd;
//	int Pmd;

	TblMidiCh[MidiCh].VoNum=Vo;
	strncpy(MidiProg[MidiCh].name,VoTbl[Vo].Name,15);
	MidiProg[MidiCh].midiProgram=Vo;

	ResetAllProgramParam(MidiCh);
	
	//発音中は音色を変えない仕様に変更 by osoumen
	/*
	Spd=CalcLim8(VoTbl[Vo].SPD,TblMidiCh[MidiCh].SPD);
	Amd=CalcLim7(VoTbl[Vo].AMD,TblMidiCh[MidiCh].AMD);
	Pmd=CalcLim7(VoTbl[Vo].PMD,TblMidiCh[MidiCh].PMD);
	
	for(i=0;i<CHMAX;i++){
		if(TblCh[i].MidiCh==MidiCh && TblCh[i].Note!=0xff){
			SendVo(i);
			SendLfo(i);
			setSPD(i,Spd);
			setAMD(i,Amd);
			setPMD(i,Pmd);
		}
	}
	 */
}


void OPMDRV::PanCng(int MidiCh,int Pan){
	int i;
	TblMidiCh[MidiCh].Pan=Pan;
	for(i=0;i<CHMAX;i++){
		if(TblCh[i].MidiCh==MidiCh /*&& TblCh[i].Note!=0xff*/){
			SendPan(i,Pan);
		}
	}
}

int OPMDRV::Save(char* tmpName){
	int i,j,ch;
	FILE *File;
	class OPDATA *pOp;
	class CHDATA *pCh;
	int TblS[4]={0,2,1,3};
	char name[4][8]={"M1","M2","C1","C2"};

	if((File=fopen(tmpName,"w"))==NULL){
		return(ERR_NOFILE);
	}
	//音色パラメータ書き出し
	fprintf(File,"//MiOPMdrv sound bank Paramer Ver2002.04.22 \n");
	fprintf(File,"//LFO: LFRQ AMD PMD WF NFRQ\n");
	fprintf(File,"//@:[Num] [Name]\n");
	fprintf(File,"//CH: PAN	FL CON AMS PMS SLOT NE\n");
	fprintf(File,"//[OPname]: AR D1R D2R	RR D1L	TL	KS MUL DT1 DT2 AMS-EN\n");

	for(ch=0;ch<VOMAX;ch++){
		pCh=&(VoTbl[ch]);
		fprintf(File,"\n@:%d %s\n",ch,pCh->Name);
		fprintf(File,"LFO:%3d %3d %3d %3d %3d\n",
		pCh->SPD,pCh->AMD,pCh->PMD,pCh->WF,pCh->NFQ);
		fprintf(File,"CH:%3d %3d %3d %3d %3d %3d %3d\n",
		pCh->F_PAN,pCh->FL,pCh->CON,pCh->AMS,pCh->PMS,pCh->OpMsk,pCh->F_NE);
		for(i=0;i<4;i++){
			j=TblS[i];
			pOp=&(pCh->Op[j]);
			fprintf(File,"%s:%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n",
			 name[j],pOp->AR,pOp->D1R,pOp->D2R,pOp->RR,pOp->D1L,pOp->TL,
			 pOp->KS,pOp->MUL,pOp->DT1,pOp->DT2,pOp->F_AME);
		}
	}
	fclose(File);
	return(ERR_NOERR);
}
int OPMDRV::Load(const char* tmpName){
	int i,errMsk=0,Nline;
	FILE *File;
	char	line[256];
	char name[64];
	char *pline;
	int CuVoNum;
	class OPDATA *pOp;
	class CHDATA *pCh;
	CuVoNum=0;
	pCh=&(VoTbl[0]);
	pOp=&(VoTbl[0].Op[0]);
	if((File=fopen(tmpName,"r"))==NULL){
		return(ERR_NOFILE);
	}
	//音色パラメータ読み込み
	Nline=0;
	while(fgets(line,256,File)!=NULL){
		Nline++;
		//fprintf(stderr,"line %d:[%s]",Nline,line);
	
		pline=line;
		while(*pline!=0 && *pline!=0x0a){				//行末まで解析繰り返す
			//fprintf(stderr,"*[%c]=%02x\n",*pline,*pline);
			if(strncmp("//",pline,2)==0){
				while(*pline!=0){pline++;}
				//fprintf(stderr,"// comment\n");
				errMsk|=0x08;
			}	//コメント以降は飛ばす

			//スペースを飛ばす
			while(strncmp(" ",pline,1)==0 && pline!=(256+line)){pline++;}

			//@:音色番号設定
			if(strncmp("@:",pline,2)==0){
				pline+=2;
				if(sscanf(pline,"%d",&CuVoNum)!=1){return(ERR_LESPRM+Nline);}
				if(CuVoNum<0){CuVoNum=0;}
				if(CuVoNum>127){CuVoNum&=0x7f;}
				pCh=&(VoTbl[CuVoNum]);

				//文字列[0-9]^n + " "^mを飛ばす
				while(isdigit(*pline)){pline++;} //数字を飛ばす
				while(*pline==' '){pline++;}	 //空白を飛ばす
				strcpy(name,"no Name");
				sscanf(pline,"%[^\n]s",name);
				name[15]='\0';
				strncpy(pCh->Name,name,16);
				errMsk|=0x01;
				while(*pline!=0 && *pline!=0x0a){pline++;}//行末までとばす
			}
			if(strncmp("CH:",pline,3)==0){
				pline+=3;
				if(sscanf(pline,"%d %d %d %d %d %d %d",
					 &(pCh->F_PAN),&(pCh->FL),&(pCh->CON),
					 &(pCh->AMS),&(pCh->PMS),&(pCh->OpMsk),&(pCh->F_NE))
					 !=7){return(ERR_LESPRM+Nline);}
				errMsk|=0x02;
				//fprintf(stderr,"CH exec:");
				while(isdigit(*pline)||*pline==' '){pline++;}
			}

			if(strncmp("LFO:",pline,4)==0){
				pline+=4;
				if(sscanf(pline,"%d %d %d %d %d",
				 &(pCh->SPD),&(pCh->AMD),&(pCh->PMD),&(pCh->WF),&pCh->NFQ)
				!=5){return(ERR_LESPRM+Nline);}
				errMsk|=0x04;
				while(isdigit(*pline)||*pline==' '){pline++;}
			}
			i=-1;
			if(strncmp("M1:",pline,3)==0){i=0;pline+=3;errMsk|=0x10;}
			if(strncmp("M2:",pline,3)==0){i=1;pline+=3;errMsk|=0x20;}
			if(strncmp("C1:",pline,3)==0){i=2;pline+=3;errMsk|=0x40;}
			if(strncmp("C2:",pline,3)==0){i=3;pline+=3;errMsk|=0x80;}
			if(i!=-1){
				pOp=&(VoTbl[CuVoNum].Op[i]);
				if(sscanf(pline,"%d %d %d %d %d %d %d %d %d %d %d",
				 &(pOp->AR),&(pOp->D1R),&(pOp->D2R),&(pOp->RR),&(pOp->D1L),&(pOp->TL),
				 &(pOp->KS),&(pOp->MUL),&(pOp->DT1),&(pOp->DT2),&(pOp->F_AME))
				 !=11){return(ERR_LESPRM+Nline);}
				while(isdigit(*pline)||*pline==' '){pline++;}
			}

			if((errMsk&0xff)==0){
				//fprintf(stderr,"OPMDRV.Load:Syntax Error in %d\n",Nline);
				fclose(File);
				return(ERR_SYNTAX+Nline);
			}else{errMsk=0;}
		}
	}
	fclose(File);
	return(ERR_NOERR);
}

//OPM chanel 管理 テーブル
int OPMDRV::ChkEnCh(void){
	if(WaitCh.size()==0){
		return(-1);
	}else{
		return(WaitCh.front());
	}
}

int OPMDRV::AtachCh(void){
//チャンネルを割り当て
	int NewCh;
	NewCh=ChkEnCh();
	if(NewCh==-1){
		 //空きチャンネルがない場合,先頭をラストにする。(一番古い発音を上書きする)
		NewCh=PlayCh.front();
		PlayCh.pop_front();
	}else{
		WaitCh.pop_front();
	}
	PlayCh.push_back(NewCh);
	return(NewCh);
}

void OPMDRV::DelCh(int ch){
 //チャンネルを削除
	//TblCh[ch].MidiCh=0xff;	//キーオフ後も各種パラメータを変更出来るように残しておく
	TblCh[ch].Note=0xff;		//発音中かどうかの判定に使用
	PlayCh.remove(ch);
	WaitCh.push_back(ch);

}

int OPMDRV::ChkSonCh(int MidiCh,int Note){
//MidiChとノート番号より発音中のスロットを探す。
//Note=0xffはワイルドカード
	list<int>::iterator plist;
	if(PlayCh.size()==0){return(-1);}
	plist=PlayCh.begin();
	while(plist!=PlayCh.end()){
	if((MidiCh==TblCh[*plist].MidiCh) && (TblCh[*plist].Note != 0xff)){
		if(Note==0xff || Note==TblCh[*plist].Note){
			return(*plist);
		}
	}
	plist++;
	}
	return(-1);
}

int OPMDRV::NonCh(int MidiCh,int Note,int Vel,int Slot ){
	int ch=-1;
//MidiChとノート、ベロシティで発音する。
	if(ChkSonCh(MidiCh,Note)!=-1){return(-1);}//既に同じチャンネル、音程で発音されていたら発音しない

	if ( Slot == -1 ) {
		list<int> goodCh;
		list<int>::iterator insertIt = goodCh.begin();
		list<int>::iterator chIt = WaitCh.begin();
		while ( chIt != WaitCh.end() && mAlternatePolyMode == false ) {
			//同じMidiChまたはProgNo.で発音しているスロットを優先
			if ( (TblCh[*chIt].MidiCh == MidiCh) && (TblCh[*chIt].VoNum == TblMidiCh[MidiCh].VoNum) ) {
				//両方一致の場合は最優先
				goodCh.insert(insertIt, *chIt);
				insertIt++;
				chIt = WaitCh.erase(chIt);
				continue;
			}
			else if ( TblCh[*chIt].MidiCh == MidiCh ) {
				//MIDIChだけ一致の場合は優先度を下げる
				goodCh.push_back(*chIt);
				chIt = WaitCh.erase(chIt);
				continue;
			}
			chIt++;
		}
		if ( goodCh.empty() ) {
			ch=AtachCh();	//リストから使えるスロットを探し割り当て
		}
		else {
			//優先度の順に並んでいるリストの先頭から一つ取り出す
			ch=goodCh.front();
			goodCh.pop_front();
			PlayCh.push_back(ch);
			//残りはWaitChに戻す
			while ( goodCh.size() > 0 ) {
				int popCh = goodCh.front();
				goodCh.pop_front();
				WaitCh.push_back(popCh);
			}
		}
	}
	else {
		//スロット指定発音
		ch = Slot & 7;
		PlayCh.remove(ch);
		WaitCh.remove(ch);
		PlayCh.push_back(ch);
	}
		
	TblCh[ch].Note=Note;
	TblCh[ch].CopyOfNote=Note;
	TblCh[ch].Vel=Vel;
	TblCh[ch].MidiCh=MidiCh;
	TblCh[ch].VoNum=TblMidiCh[MidiCh].VoNum;
	return(ch);
}

int OPMDRV::NoffCh(int MidiCh,int Note){
	int ch;
//MidiChとノートで消音する。
	if((ch=ChkSonCh(MidiCh,Note))!=-1){//消音されてないか？
		DelCh(ch);
	}

#ifdef MyDEBUG
	fprintf(stdout,"NoffSlot:ch=%x Note=%x MidiCh=%x\n",ch,Note,Ch);
#endif

return(ch);
}


unsigned char OPMDRV::getPrm(int index){
		class CHDATA *pVo;
	pVo=&VoTbl[CuProgNum];

	switch (index)
	{
		case kCon:return(pVo->CON);
		case kFL:return(pVo->FL);
		case kAMS:return(pVo->AMS);
		case kPMS:return(pVo->PMS);
		case kNE:return((pVo->F_NE==128)?1:0);
		case kSPD:return(pVo->SPD);
		case kPMD:return(pVo->PMD);
		case kAMD:return(pVo->AMD);
		case kWF:return(pVo->WF);
		case kNFQ:return(pVo->NFQ);
		case kOpMsk:return(pVo->OpMsk);
		case kM1TL:return(pVo->Op[0].TL);
		case kM2TL:return(pVo->Op[1].TL);
		case kC1TL:return(pVo->Op[2].TL);
		case kC2TL:return(pVo->Op[3].TL);
		case kM1D1L:return(pVo->Op[0].D1L);
		case kM2D1L:return(pVo->Op[1].D1L);
		case kC1D1L:return(pVo->Op[2].D1L);
		case kC2D1L:return(pVo->Op[3].D1L);
		case kM1AR:return(pVo->Op[0].AR);
		case kM2AR:return(pVo->Op[1].AR);
		case kC1AR:return(pVo->Op[2].AR);
		case kC2AR:return(pVo->Op[3].AR);
		case kM1D1R:return(pVo->Op[0].D1R);
		case kM2D1R:return(pVo->Op[1].D1R);
		case kC1D1R:return(pVo->Op[2].D1R);
		case kC2D1R:return(pVo->Op[3].D1R);
		case kM1D2R:return(pVo->Op[0].D2R);
		case kM2D2R:return(pVo->Op[1].D2R);
		case kC1D2R:return(pVo->Op[2].D2R);
		case kC2D2R:return(pVo->Op[3].D2R);
		case kM1RR:return(pVo->Op[0].RR);
		case kM2RR:return(pVo->Op[1].RR);
		case kC1RR:return(pVo->Op[2].RR);
		case kC2RR:return(pVo->Op[3].RR);
		case kM1KS:return(pVo->Op[0].KS);
		case kM2KS:return(pVo->Op[1].KS);
		case kC1KS:return(pVo->Op[2].KS);
		case kC2KS:return(pVo->Op[3].KS);
		case kM1DT1:return(pVo->Op[0].DT1);
		case kM2DT1:return(pVo->Op[1].DT1);
		case kC1DT1:return(pVo->Op[2].DT1);
		case kC2DT1:return(pVo->Op[3].DT1);
		case kM1MUL:return(pVo->Op[0].MUL);
		case kM2MUL:return(pVo->Op[1].MUL);
		case kC1MUL:return(pVo->Op[2].MUL);
		case kC2MUL:return(pVo->Op[3].MUL);
		case kM1DT2:return(pVo->Op[0].DT2);
		case kM2DT2:return(pVo->Op[1].DT2);
		case kC1DT2:return(pVo->Op[2].DT2);
		case kC2DT2:return(pVo->Op[3].DT2);
		case kM1F_AME:return((pVo->Op[0].F_AME==128)?1:0);
		case kM2F_AME:return((pVo->Op[1].F_AME==128)?1:0);
		case kC1F_AME:return((pVo->Op[2].F_AME==128)?1:0);
		case kC2F_AME:return((pVo->Op[3].F_AME==128)?1:0);
//OpMsk
		case kMskM1:return((pVo->OpMsk&0x08)?1:0);
		case kMskC1:return((pVo->OpMsk&0x10)?1:0);
		case kMskM2:return((pVo->OpMsk&0x20)?1:0);
		case kMskC2:return((pVo->OpMsk&0x40)?1:0);
//Con
		case kCon0:case kCon1:case kCon2:case kCon3:
		case kCon4:case kCon5:case kCon6:case kCon7:
			return((pVo->CON==static_cast<unsigned int>(index-kCon0))?1:0);

//WF
		case kWf0:case kWf1:case kWf2: case kWf3:
			return((pVo->WF==static_cast<unsigned int>(index-kWf0))?1:0);
	}
		return(0);
}


void OPMDRV::setPrm(int index,unsigned char data){
	class CHDATA *pVo;
	int i;
	pVo=&VoTbl[CuProgNum];
	i=index;
	switch (index)
	{
		case kCon:pVo->CON=data;break;
				case kFL:pVo->FL=data;break;
		case kAMS:pVo->AMS=data;break;
		case kPMS:pVo->PMS=data;break;
				case kNE:pVo->F_NE=(data==1)?128:0;break;
		case kSPD:pVo->SPD=data;break;
		case kPMD:pVo->PMD=data;break;
		case kAMD:pVo->AMD=data;break;
		case kWF:pVo->WF=data;break;
		case kNFQ:pVo->NFQ=data;break;
		case kOpMsk:pVo->OpMsk=data;break;
		case kM1TL:pVo->Op[0].TL=data;break;
		case kM2TL:pVo->Op[1].TL=data;break;
		case kC1TL:pVo->Op[2].TL=data;break;
		case kC2TL:pVo->Op[3].TL=data;break;
		case kM1D1L:pVo->Op[0].D1L=data;break;
		case kM2D1L:pVo->Op[1].D1L=data;break;
		case kC1D1L:pVo->Op[2].D1L=data;break;
		case kC2D1L:pVo->Op[3].D1L=data;break;
		case kM1AR:pVo->Op[0].AR=data;break;
		case kM2AR:pVo->Op[1].AR=data;break;
		case kC1AR:pVo->Op[2].AR=data;break;
		case kC2AR:pVo->Op[3].AR=data;break;
		case kM1D1R:pVo->Op[0].D1R=data;break;
		case kM2D1R:pVo->Op[1].D1R=data;break;
		case kC1D1R:pVo->Op[2].D1R=data;break;
		case kC2D1R:pVo->Op[3].D1R=data;break;
		case kM1D2R:pVo->Op[0].D2R=data;break;
		case kM2D2R:pVo->Op[1].D2R=data;break;
		case kC1D2R:pVo->Op[2].D2R=data;break;
		case kC2D2R:pVo->Op[3].D2R=data;break;
		case kM1RR:pVo->Op[0].RR=data;break;
		case kM2RR:pVo->Op[1].RR=data;break;
		case kC1RR:pVo->Op[2].RR=data;break;
		case kC2RR:pVo->Op[3].RR=data;break;
		case kM1KS:pVo->Op[0].KS=data;break;
		case kM2KS:pVo->Op[1].KS=data;break;
		case kC1KS:pVo->Op[2].KS=data;break;
		case kC2KS:pVo->Op[3].KS=data;break;
		case kM1DT1:pVo->Op[0].DT1=data;break;
		case kM2DT1:pVo->Op[1].DT1=data;break;
		case kC1DT1:pVo->Op[2].DT1=data;break;
		case kC2DT1:pVo->Op[3].DT1=data;break;
		case kM1MUL:pVo->Op[0].MUL=data;break;
		case kM2MUL:pVo->Op[1].MUL=data;break;
		case kC1MUL:pVo->Op[2].MUL=data;break;
		case kC2MUL:pVo->Op[3].MUL=data;break;
		case kM1DT2:pVo->Op[0].DT2=data;break;
		case kM2DT2:pVo->Op[1].DT2=data;break;
		case kC1DT2:pVo->Op[2].DT2=data;break;
		case kC2DT2:pVo->Op[3].DT2=data;break;
		case kM1F_AME:pVo->Op[0].F_AME=(data==1)?128:0;break;
		case kM2F_AME:pVo->Op[1].F_AME=(data==1)?128:0;break;
		case kC1F_AME:pVo->Op[2].F_AME=(data==1)?128:0;break;
		case kC2F_AME:pVo->Op[3].F_AME=(data==1)?128:0;break;
//OpMsk 
		case kMskM1:
			if(data==0){
				pVo->OpMsk&=~0x08;
			}else{
				pVo->OpMsk|=0x08;
			}
			break;
		case kMskC1:
			if(data==0){
				pVo->OpMsk&=~0x10;
			}else{
				pVo->OpMsk|=0x10;
			}
			break;

		case kMskM2:
			if(data==0){
				pVo->OpMsk&=~0x20;
			}else{
				pVo->OpMsk|=0x20;
			}
			break;

		case kMskC2:
			if(data==0){
				pVo->OpMsk&=~0x40;
			}else{
				pVo->OpMsk|=0x40;
			}
			break;
			//Con
		case kCon0:case kCon1:case kCon2:case kCon3:
		case kCon4:case kCon5:case kCon6:case kCon7:
			if(data==1){
				pVo->CON=(i-kCon0);
			}
			
			break;
			//WF
		case kWf0:case kWf1:case kWf2: case kWf3:
			if(data==1){
				pVo->WF=(i-kWf0);
			}
			break;
			
	}
}

void OPMDRV::setPoly(bool poly){
	int i;
	for(i=0;i<8;i++){
		NoteAllOff(i);
	}
	PolyMono=poly;
}

long OPMDRV::getMidiProgram(long channel,MidiProgramName *curProg){
	MidiProg[channel].parentCategoryIndex=-1;	
	*curProg=MidiProg[channel];
	return(0);
}

void OPMDRV::setPortaSpeed(int MidiCh,int speed){
	if(speed<0){speed=0;}
	if(speed>127){speed=127;}
	TblMidiCh[MidiCh].PortaSpeed=speed;
}

void OPMDRV::setPortaOnOff(int MidiCh,bool onoff){
	TblMidiCh[MidiCh].PortaOnOff=onoff;
}

void OPMDRV::setPortaCtr(int MidiCh,int Note){
	if(Note<0){Note=0;}
	if(Note>127){Note=127;}
	TblMidiCh[MidiCh].BfNote=Note;
}
void OPMDRV::setLFOdelay(int MidiCh,int data){
	if(data<0){data=0;}
	if(data>127){data=127;}
	TblMidiCh[MidiCh].LFOdelay=data;
}

void OPMDRV::setCcTL_L(int MidiCh,int data,int OpNum){
	TblMidiCh[MidiCh].OpTL[OpNum]&=0xff00;
	TblMidiCh[MidiCh].OpTL[OpNum]|=data<<1;
}

void OPMDRV::setCcTL_H(int MidiCh,int data,int OpNum){
	int ch;
	int Tl;
	int tmp;

	OpNum&=0x03;
	tmp=TblMidiCh[MidiCh].OpTL[OpNum];
	tmp&=0x00ff;
	tmp|=data<<8;
	TblMidiCh[MidiCh].OpTL[OpNum]=tmp;
	
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	
	Tl=CalcLimTL(ChData.Op[OpNum].TL,tmp);

	for(ch=0;ch<CHMAX;ch++){
		if(MidiCh==TblCh[ch].MidiCh && TblCh[ch].Note!=0xff){
			setTL(ch,Tl,OpNum);
		}
	}
}

float OPMDRV::ApplyVelocitySens( float vel, int velSens )
{
	//velSens=127のときvel=velになる
	float gamma = expf((127 - velSens)/56.0f);
	float value = 127.0f * powf(vel/127.0f, 1.0f/gamma);
	return value;
}

float OPMDRV::CalcTLFromAmp( float amp )
{
	return ( -8.0f * log10f(amp) * log_2_inv);
}

int OPMDRV::calcTLAdjust(int ch, int OpNum)
{
	CHDATA	ChData;
	mergeControlChange(&ChData, TblCh[ch].VoNum, TblCh[ch].MidiCh);
	
	int	MidiCh = TblCh[ch].MidiCh;
	int vol		= 127;
	int vol2	= 127;
	
	//実機OPMにはボリューム、ベロシティが無いので、TLの調整で実現
	if ( (IsRealMode() || mIsDumperRunning) && (is_vol_set[ChData.CON][OpNum] != 0) ) {
		vol = TblMidiCh[MidiCh].Vol;
		vol2 = TblMidiCh[MidiCh].Vol2;
	}
	
	float	vel	= static_cast<float>(TblCh[ch].Vel);
	unsigned int	velSens = TblMidiCh[MidiCh].OpVelSence[OpNum];
	if ( velSens == CCVALUE_NONSET ) {
		velSens = is_vol_set[ChData.CON][OpNum] * 127;
	}
	if ( velSens == 0 ) {
		//Opがベロシティ感度=0なら常に最大値
		vel = 127;
	}
	else {
		vel = ApplyVelocitySens( vel, velSens );
	}
	
	//ボリュームの二次関数をTL値に変換
	float	total_gain = .0f;
	float	amp;
	if ( vel != 0 ) {
		amp = vel / 127.0f;
		amp *= amp;
		total_gain += CalcTLFromAmp(amp);
	}
	if ( vol != 0 && vol2 != 0 ) {
		amp = (vol * vol2) / (127.0f * 127.0f);
		amp *= amp;
		total_gain += CalcTLFromAmp(amp);
	}
	else {
		total_gain += 127.0f;	//設定値が０の場合は最小の音量に設定
	}
	
	//コンプレッサーによる音量変更を適用
	if (is_vol_set[ChData.CON][OpNum] != 0) {
		total_gain += TblCh[ch].reduction;
	}
	
	int op_bit[4] = {SWENV_DEST_OP1, SWENV_DEST_OP3, SWENV_DEST_OP2, SWENV_DEST_OP4 };
	
	//ソフトウェアLFOによる音量値を反映
	for (int i=0; i<SWLFO_NUM; i++) {
		if ( ((TblMidiCh[MidiCh].SwLfo[i].Dest & SWENV_DEST_AMP)*is_vol_set[ChData.CON][OpNum]) |
			(TblMidiCh[MidiCh].SwLfo[i].Dest & op_bit[OpNum]) ) {
			total_gain += TblCh[ch].SwLfo[i].GetAmValue(ch) / 8.0f;
		}
	}
	
	//ソフトウェアエンベロープの値を反映
	for (int i=0; i<SWENV_NUM; i++) {
		if ( ((TblMidiCh[MidiCh].SwEnv[i].Dest & SWENV_DEST_AMP)*is_vol_set[ChData.CON][OpNum]) |
			(TblMidiCh[MidiCh].SwEnv[i].Dest & op_bit[OpNum]) ) {
			//音量値もしくはオペレータのビットが立っていたら、音量調整を行う
			total_gain += ((1024 - TblCh[ch].Env[i].GetEnvValue()) * TblMidiCh[MidiCh].SwEnv[i].Depth) / 1024.0f;
		}
	}
	
	return (int)(total_gain + 0.5f);
}

void OPMDRV::setTL(int ch,int Tl,int OpNum){
	Tl += calcTLAdjust(ch,OpNum);
	if ( Tl > 127 ) {
		Tl = 127;
	}
	if ( Tl < 0 ) {
		Tl = 0;
	}
//	printf( "TL=%d\n",Tl);

	switch(OpNum&0x03){
		case 0x00:
			_iocs_opmset(0x60+ch,Tl);
			break;
		case 0x01:
			_iocs_opmset(0x68+ch,Tl);
			break;
		case 0x02:
			_iocs_opmset(0x70+ch,Tl);
			break;
		case 0x03:
			_iocs_opmset(0x78+ch,Tl);
			break;
	}
}

void OPMDRV::setCcLFQ_L(int MidiCh,int data){
	TblMidiCh[MidiCh].SPD&=0xff00;
	TblMidiCh[MidiCh].SPD|=data<<1;
}
void OPMDRV::setCcLFQ_H(int MidiCh,int data){
	int ch;
	int Spd;
	int tmp;

	tmp=TblMidiCh[MidiCh].SPD;
	tmp&=0x00ff;
	tmp|=data<<8;
	TblMidiCh[MidiCh].SPD=tmp;

	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	Spd=CalcLim8(ChData.SPD,tmp);

	for(ch=0;ch<8;ch++){
		if(MidiCh==TblCh[ch].MidiCh && TblCh[ch].Note!=0xff){
			setSPD(ch,Spd);
		}
	}
}

void OPMDRV::setSPD(int ch,int Spd){
	if ( IsRealMode() == false || TblCh[ch].MidiCh == 0 ) {
		_iocs_opmset(0x00,ch);
		_iocs_opmset(0x18,Spd);
	}
}

void OPMDRV::setCcPMD_L(int MidiCh,int data){
	TblMidiCh[MidiCh].PMD&=0xff00;
	TblMidiCh[MidiCh].PMD|=data<<1;
}
void OPMDRV::setCcPMD_H(int MidiCh,int data){
	int ch;
	int Pmd;
	int tmp;

	tmp=TblMidiCh[MidiCh].PMD;
	tmp&=0x00ff;
	tmp|=data<<8;
	TblMidiCh[MidiCh].PMD=tmp;

	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	Pmd=CalcLim7(ChData.PMD,tmp);

	for(ch=0;ch<8;ch++){
		if(MidiCh==TblCh[ch].MidiCh && TblCh[ch].Note!=0xff){
			setPMD(ch,Pmd);
		}
	}
}

void OPMDRV::setPMD(int ch,int Pmd){
	if ( IsRealMode() == false || TblCh[ch].MidiCh == 0 ) {
		_iocs_opmset(0x00,ch);
		_iocs_opmset(0x19,(0x80|Pmd));
	}
}

void OPMDRV::setCcAMD_L(int MidiCh,int data){
	TblMidiCh[MidiCh].AMD&=0xff00;
	TblMidiCh[MidiCh].AMD|=data<<1;
}
void OPMDRV::setCcAMD_H(int MidiCh,int data){
	int ch;
	int Amd;
	int tmp;
	tmp=TblMidiCh[MidiCh].AMD;
	tmp&=0x00ff;
	tmp|=data<<8;
	TblMidiCh[MidiCh].AMD=tmp;

	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	Amd=CalcLim7(ChData.AMD,tmp);

	for(ch=0;ch<8;ch++){
		if(MidiCh==TblCh[ch].MidiCh && TblCh[ch].Note!=0xff){
			setAMD(ch,Amd);
		}
	}
}

void OPMDRV::setAMD(int ch,int Amd){
	if ( IsRealMode() == false || TblCh[ch].MidiCh == 0 ) {
		_iocs_opmset(0x00,ch);
		_iocs_opmset(0x19,Amd);
	}
}

int OPMDRV::CalcLim8(int VoData,int DifData){
	int ret;
	ret=VoData+(((DifData&0x3ffff)-0x4000)>>7);
	if(ret>0xff){ret=0xff;}
	if(ret<0x0){ret=0x0;}
	return(ret);
}
int OPMDRV::CalcLim7(int VoData,int DifData){
	int ret;
	ret=VoData+(((DifData&0x7fff)-0x4000)>>8);
	if(ret>0x7f){ret=0x7f;}
	if(ret<0x0){ret=0x0;}
	return(ret);
}
int OPMDRV::CalcLimTL(int VoData,int DifData){
	int ret;
	ret=VoData+(((DifData&0x7fff)-0x4000)>>8);
	if(ret>0x7f){ret=0x7f;}
	if(ret<0x0){ret=0x0;}
	return(ret);
}

void OPMDRV::BeginS98Log()
{
	mFrameStartPos = 0;
	//mTickPerSec = mTempo*8;
	mTickPerSec = 1000;
	m_pRegDumper->SetResolution(1, static_cast<int>(mTickPerSec));
	m_pRegDumper->BeginDump(0);
	mIsDumperRunning = true;
}

void OPMDRV::MarkS98Loop()
{
	if ( mIsDumperRunning ) {
		m_pRegDumper->MarkLoopPoint();
	}
}

void OPMDRV::EndS98Log()
{
	if ( mIsDumperRunning ) {
		m_pRegDumper->EndDump( static_cast<int>(((mFrameStartPos+SampleTime) * mTickPerSec) / Samprate + 0.5) );
		mIsDumperRunning = false;
	}
}

int OPMDRV::SaveS98Log(const char *path)
{
	if ( *path == 0 ) {
		return(ERR_NOFILE);
	}
	if ( CanSaveS98() == false ) {
		return(ERR_NOFILE);
	}
	int clock[] = {3580000, 4000000, 3579545};
	m_pRegDumper->SaveToFile(path, clock[OpmClock]);
	return(ERR_NOERR);
}

bool OPMDRV::CanSaveS98()
{
	if ( mIsDumperRunning == false && m_pRegDumper->IsEnded() ) {
		return true;
	}
	return false;
}

void OPMDRV::FrameTask(int frameSamples)
{
	mFrameStartPos += frameSamples;
	if ( m_pRealChip ) {
		m_pRealChip->FrameTask();
	}
}

inline bool OPMDRV::IsRealMode()
{
	return (m_pRealChip != NULL) ? true:false;
}

bool OPMDRV::HandleControlChange( int MidiCh, int CCNum, int value )
{
	if ( TblMidiCh[MidiCh].CcMode == CCMODE_ORIGINAL ) {
		switch(CCNum){
			case 0x01://LFO AMD_H
			case 0x0c:
				setCcAMD_H(MidiCh,value);
				break;
			case 0x21://LFO AMD_L
			case 0x2c:
				setCcAMD_L(MidiCh,value);
				break;
			case 0x02://LFO PMD_H
			case 0x0d:
				setCcPMD_H(MidiCh,value);
				break;
			case 0x22://LFO PMD_L
			case 0x2d:
				setCcPMD_L(MidiCh,value);
				break;
				
			case 0x03://LFO rate_H
				setCcLFQ_H(MidiCh,value);
				break;
			case 0x23://LFO rate_L
				setCcLFQ_L(MidiCh,value);
				break;
				
			case 0x05://PortaTime
				setPortaSpeed(MidiCh,value);
				break;
			case 0x06://DataH Entr
				setData(MidiCh,value);
				break;
			case 0x0A://pan
				PanCng(MidiCh,value);
				break;
				
			case 0x0B://Expression
				Vol2Cng(MidiCh, value);
				break;
				
			case 0x10://TL0_H
				setCcTL_H(MidiCh,value,0);
				break;
			case 0x30://TL0_L
				setCcTL_L(MidiCh,value,0);
				break;
			case 0x11://TL1_H
				setCcTL_H(MidiCh,value,1);
				break;
			case 0x31://TL1_L
				setCcTL_L(MidiCh,value,1);
				break;
			case 0x12://TL2_H
				setCcTL_H(MidiCh,value,2);
				break;
			case 0x32://TL2_L
				setCcTL_L(MidiCh,value,2);
				break;
			case 0x13://TL3_H
				setCcTL_H(MidiCh,value,3);
				break;
			case 0x33://TL3_L
				setCcTL_L(MidiCh,value,3);
				break;
				
			case 0x07://ch-vol
				VolCng(MidiCh,value);
				break;

			case 0x41://portaOnOff
				setPortaOnOff(MidiCh,(value>=64)?true:false);
				break;
			case 0x4C://LFO rate
				setCcLFQ_H(MidiCh,value);
				//updateDisplay();
				//if(editor)((class VOPMEdit *)editor)->update();
				break;
			case 0x4E://LFO delay
				setLFOdelay(MidiCh,value);
				break;
			case 0x54://Porta Ctr
				setPortaCtr(MidiCh,value);
				break;
			case 0x62://NRPNL
				setNRPNL(MidiCh,value);
				break;
			case 0x63://NRPNH
				setNRPNH(MidiCh,value);
				break;
				
			case 0x64://RPNL
				setRPNL(MidiCh,value);
				break;
			case 0x65://RPNH
				setRPNH(MidiCh,value);
				break;
			case 0x78://Force off
				ForceAllOff(MidiCh);
				break;
			case 0x79:
				ResetAllControllers(MidiCh);
				break;
			case 0x7A://Local On/Off
				if ( value == 127 ) {
					SetLocalOn(true);
				}
				if ( value == 0 ) {
					SetLocalOn(false);
				}
				break;
			case 0x7B://All Note Off
				NoteAllOff(MidiCh);
				break;
			case 0x7E://Mono
				setPoly(false);
				break;
			case 0x7F://Poly
				setPoly(true);
				if ( value >= 64 ) {
					setChAllocMode(true);
				}
				else {
					setChAllocMode(false);
				}
				break;
							
			default:
				return false;			
		}
	}
	else {
		switch(CCNum){
			case 0x01://SPD(LFO FRQ)
				setCcFRQ_H(MidiCh,value);
				break;
			case 0x02://PMD
				setCcPMD(MidiCh,value);
				break;
			case 0x03://AMD
				setCcAMD(MidiCh,value);
				break;
			case 0x05://PortaTime
				setPortaSpeed(MidiCh,value);
				break;
			case 0x06://DataH Entr
				setData(MidiCh,value);
				break;
			case 0x07://ch-vol
				VolCng(MidiCh,value);
				break;
				
			case 0x3F://SWLFO1_DEST
				setCcSwLFO1_Dest(MidiCh,value);
				break;
			case 0x08://SWPLFO_FRQ
				setCcSwLFO1_FRQ(MidiCh,value);
				break;
			case 0x04://SWPLFO_PMD
				setCcSwLFO1_Depth(MidiCh,value);
				break;
			case 0x0D://SWPLFO_SYNC
				setCcSwLFO1_Sync(MidiCh,value);
				break;
			case 0x09://SWPLFO_WF
				setCcSwLFO1_WF(MidiCh,value);
				break;
				
			case 0x44://SWLFO2_DEST
				setCcSwLFO2_Dest(MidiCh,value);
				break;
			case 0x22://SWLFO2_FRQ
				setCcSwLFO2_FRQ(MidiCh,value);
				break;
			case 0x23://SWLFO2_WF
				setCcSwLFO2_WF(MidiCh,value);
				break;
			case 0x24://SWLFO2_DEPTH
				setCcSwLFO2_Depth(MidiCh,value);
				break;
			case 0x25://SWLFO2_SYNC
				setCcSwLFO2_Sync(MidiCh,value);
				break;
				
			case 0x0A://pan
				PanCng(MidiCh,value);
				break;
				
			case 0x0B://Expression
				Vol2Cng(MidiCh, value);
				break;
				
			case 0x0C://WF
				setCcWF(MidiCh,value);
				break;
			case 0x0E://CON
				setCcCON(MidiCh,value);
				break;
			case 0x0F://FL
				setCcFL(MidiCh,value);
				break;
			case 0x10://TL1
				setCcTL(MidiCh,value,0);
				break;
			case 0x11://TL3
				setCcTL(MidiCh,value,2);
				break;
			case 0x12://TL2
				setCcTL(MidiCh,value,1);
				break;
			case 0x13://TL4
				setCcTL(MidiCh,value,3);
				break;
			case 0x14://MUL1
				setCcMUL(MidiCh,value,0);
				break;
			case 0x15://MUL3
				setCcMUL(MidiCh,value,2);
				break;
			case 0x16://MUL2
				setCcMUL(MidiCh,value,1);
				break;
			case 0x17://MUL4
				setCcMUL(MidiCh,value,3);
				break;
			case 0x18://DT1_1
				setCcDT1(MidiCh,value,0);
				break;
			case 0x19://DT1_3
				setCcDT1(MidiCh,value,2);
				break;
			case 0x1A://DT1_2
				setCcDT1(MidiCh,value,1);
				break;
			case 0x1B://DT1_4
				setCcDT1(MidiCh,value,3);
				break;
			case 0x1C://DT2_1
				setCcDT2(MidiCh,value,0);
				break;
			case 0x1D://DT2_3
				setCcDT2(MidiCh,value,2);
				break;
			case 0x1E://DT2_2
				setCcDT2(MidiCh,value,1);
				break;
			case 0x1F://DT2_4
				setCcDT2(MidiCh,value,3);
				break;
			case 0x21://SPD_L
				setCcFRQ_L(MidiCh,value);
				break;
				
			case 0x27://KS 1
				setCcKS(MidiCh,value,0);
				break;
			case 0x28://KS 3
				setCcKS(MidiCh,value,2);
				break;
			case 0x29://KS 2
				setCcKS(MidiCh,value,1);
				break;
			case 0x2A://KS 4
				setCcKS(MidiCh,value,3);
				break;
			case 0x2B://AR 1
				setCcAR(MidiCh,value,0);
				break;
			case 0x2C://AR 3
				setCcAR(MidiCh,value,2);
				break;
			case 0x2D://AR 2
				setCcAR(MidiCh,value,1);
				break;
			case 0x2E://AR 4
				setCcAR(MidiCh,value,3);
				break;
			case 0x2F://D1R 1
				setCcD1R(MidiCh,value,0);
				break;
			case 0x30://D1R 3
				setCcD1R(MidiCh,value,2);
				break;
			case 0x31://D1R 2
				setCcD1R(MidiCh,value,1);
				break;
			case 0x32://D1R 4
				setCcD1R(MidiCh,value,3);
				break;
			case 0x33://D2R 1
				setCcD2R(MidiCh,value,0);
				break;
			case 0x34://D2R 3
				setCcD2R(MidiCh,value,2);
				break;
			case 0x35://D2R 2
				setCcD2R(MidiCh,value,1);
				break;
			case 0x36://D2R 4
				setCcD2R(MidiCh,value,3);
				break;
			case 0x37://D1L 1
				setCcD1L(MidiCh,value,0);
				break;
			case 0x38://D1L 3
				setCcD1L(MidiCh,value,2);
				break;
			case 0x39://D1L 2
				setCcD1L(MidiCh,value,1);
				break;
			case 0x3A://D1L 4
				setCcD1L(MidiCh,value,3);
				break;
			case 0x3B://RR 1
				setCcRR(MidiCh,value,0);
				break;
			case 0x3C://RR 3
				setCcRR(MidiCh,value,2);
				break;
			case 0x3D://RR 2
				setCcRR(MidiCh,value,1);
				break;
			case 0x3E://RR 4
				setCcRR(MidiCh,value,3);
				break;
			
			case 0x41://portaOnOff
				setPortaOnOff(MidiCh,(value>=64)?true:false);
				break;
			
			case 0x46://AME 1
				setCcAME(MidiCh,value,0);
				break;
			case 0x47://AME 3
				setCcAME(MidiCh,value,2);
				break;
			case 0x48://AME 2
				setCcAME(MidiCh,value,1);
				break;
			case 0x49://AME 4
				setCcAME(MidiCh,value,3);
				break;
				
			case 0x4A://Force LFO Reset
				forceLFOReset(MidiCh,value);
				break;
				
			case 0x4B://PMS
				setCcPMS(MidiCh, value);
				break;
			case 0x4C://AMS
				setCcAMS(MidiCh, value);
				break;
			case 0x4D://F_PAN
				if ( value == 1 ) {
					PanCng(MidiCh,0);	//L
				}
				else if ( value == 2 ) {
					PanCng(MidiCh,127);	//R
				}
				else if ( value == 3 ) {
					PanCng(MidiCh,64);	//LR
				}
				break;
			
			case 0x4E://LFO delay
				setLFOdelay(MidiCh,value);
				break;
			
			case 0x4F://CurrentSwEnv
				setCcCurrentSwEnv(MidiCh,value);
				break;
				
			case 0x50://NE
				setCcNE(MidiCh,value);
				break;
				
			case 0x51://Pitch Bend Range
				setCcPBRange(MidiCh,value);
				break;
				
			case 0x52://NFQ
				setCcNFQ(MidiCh,value);
				break;
			
			case 0x53://LFO Sync
				setCcLFOSync(MidiCh, value);
				break;
				
			case 0x54://Porta Ctr
				setPortaCtr(MidiCh,value);
				break;
				
				
			case 0x55://LFO1 Delay
				setCcSwLFO1_Delay(MidiCh,value);
				break;
			case 0x56://LFO2 Delay
				setCcSwLFO2_Delay(MidiCh,value);
				break;
				
			case 0x57://Velocity Sens OP1
				setCcVelocitySens(MidiCh,value,0);
				break;
			case 0x58://Velocity Sens OP2
				setCcVelocitySens(MidiCh,value,2);
				break;
			case 0x59://Velocity Sens OP3
				setCcVelocitySens(MidiCh,value,1);
				break;
			case 0x5A://Velocity Sens OP4
				setCcVelocitySens(MidiCh,value,3);
				break;
				
			case 0x62://NRPNL
				setNRPNL(MidiCh,value);
				break;
			case 0x63://NRPNH
				setNRPNH(MidiCh,value);
				break;
				
			case 0x64://RPNL
				setRPNL(MidiCh,value);
				break;
			case 0x65://RPNH
				setRPNH(MidiCh,value);
				break;
				
			case 0x66://SWENV_DEST
				setCcSwEnvDest(MidiCh,value,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x5F://SWENV_DEPTH
				setCcSwEnvDepth(MidiCh,value,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x67://SWENV_LOOP
				setCcSwEnvLoop(MidiCh,value,TblMidiCh[MidiCh].CurrentEnvSel);
				break;				
			case 0x68://SWENV_TIME_1
				setCcSwEnvTime(MidiCh,value,0,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x69://SWENV_VALUE_1
				setCcSwEnvValue(MidiCh,value,0,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x6A://SWENV_TIME_2
				setCcSwEnvTime(MidiCh,value,1,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x6B://SWENV_VALUE_2
				setCcSwEnvValue(MidiCh,value,1,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x6C://SWENV_TIME_3
				setCcSwEnvTime(MidiCh,value,2,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x6D://SWENV_VALUE_3
				setCcSwEnvValue(MidiCh,value,2,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x6E://SWENV_TIME_4
				setCcSwEnvTime(MidiCh,value,3,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x6F://SWENV_VALUE_4
				setCcSwEnvValue(MidiCh,value,3,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x70://SWENV_TIME_5
				setCcSwEnvTime(MidiCh,value,4,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
			case 0x71://SWENV_VALUE_5
				setCcSwEnvValue(MidiCh,value,4,TblMidiCh[MidiCh].CurrentEnvSel);
				break;
				
			case 0x5E://CompRatio
				setCcCompRatio(MidiCh,value);
				break;
			case 0x72://CompInputCh
				setCcCompInputCh(MidiCh,value);
				break;
			case 0x73://CompThreshold
				setCcCompThreshold(MidiCh,value);
				break;
			case 0x74://CompAttack
				setCcCompAtk(MidiCh,value);
				break;
			case 0x75://CompRelease
				setCcCompRel(MidiCh,value);
				break;
				
			case 0x78://Force off
				ForceAllOff(MidiCh);
				break;
			case 0x79:
				ResetAllControllers(MidiCh);
				break;
			case 0x7A://Local On/Off
				if ( value == 127 ) {
					SetLocalOn(true);
				}
				if ( value == 0 ) {
					SetLocalOn(false);
				}
				break;
			case 0x7B://All Note Off
				NoteAllOff(MidiCh);
				break;
			case 0x7E://Mono
				setPoly(false);
				break;
			case 0x7F://Poly
				setPoly(true);
				if ( value >= 64 ) {
					setChAllocMode(true);
				}
				else {
					setChAllocMode(false);
				}
				break;
				
			default:
				return false;
		}
	}
	return true;
}

void OPMDRV::setCcValue(int MidiCh, int data, unsigned int &CcValue, unsigned int range, bool reverse)
{
	int max = range-1;
	range--;
	int bit_shift = 7;
	while ( range > 0 && bit_shift > 0 ) {
		range >>= 1;
		bit_shift--;
	}
	unsigned int value = data;
	if ( TblMidiCh[MidiCh].CcDirection == CCDIRECTION_NATURAL ) {
		value >>= bit_shift;
		if ( reverse ) {
			value = max - value;
		}
	}
	else {
		value &= max;
	}
	
	//printf("max=%d\n",max);
	//printf("value=%d\n",value);
#if 0
	if ( value >= range ) {
		//範囲を超えたデータは未設定状態にする
		value = CCVALUE_NONSET;
	}
#endif
	CcValue = value;
}

void OPMDRV::mergeControlChange( CHDATA *outData, int VoNum, int MidiCh )
{
	assert(outData!=NULL);
	//CCValueのデータのうち、CCVALUE_NONSETでない値をoutDataにコピーする
	
	outData->F_PAN	= (TblMidiCh[MidiCh].CCValue.F_PAN == CCVALUE_NONSET)?VoTbl[VoNum].F_PAN : TblMidiCh[MidiCh].CCValue.F_PAN;
	outData->CON	= (TblMidiCh[MidiCh].CCValue.CON == CCVALUE_NONSET)?VoTbl[VoNum].CON : TblMidiCh[MidiCh].CCValue.CON;
	outData->FL		= (TblMidiCh[MidiCh].CCValue.FL == CCVALUE_NONSET)?VoTbl[VoNum].FL : TblMidiCh[MidiCh].CCValue.FL;
	outData->AMS	= (TblMidiCh[MidiCh].CCValue.AMS == CCVALUE_NONSET)?VoTbl[VoNum].AMS : TblMidiCh[MidiCh].CCValue.AMS;
	outData->PMS	= (TblMidiCh[MidiCh].CCValue.PMS == CCVALUE_NONSET)?VoTbl[VoNum].PMS : TblMidiCh[MidiCh].CCValue.PMS;
	outData->F_NE	= (TblMidiCh[MidiCh].CCValue.F_NE == CCVALUE_NONSET)?VoTbl[VoNum].F_NE : TblMidiCh[MidiCh].CCValue.F_NE;
	for ( int j=0; j<4; j++ ) {
		outData->Op[j].TL	= (TblMidiCh[MidiCh].CCValue.Op[j].TL == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].TL : TblMidiCh[MidiCh].CCValue.Op[j].TL;
		outData->Op[j].D1L	= (TblMidiCh[MidiCh].CCValue.Op[j].D1L == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].D1L : TblMidiCh[MidiCh].CCValue.Op[j].D1L;
		outData->Op[j].AR	= (TblMidiCh[MidiCh].CCValue.Op[j].AR == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].AR : TblMidiCh[MidiCh].CCValue.Op[j].AR;
		outData->Op[j].D1R	= (TblMidiCh[MidiCh].CCValue.Op[j].D1R == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].D1R : TblMidiCh[MidiCh].CCValue.Op[j].D1R;
		outData->Op[j].D2R	= (TblMidiCh[MidiCh].CCValue.Op[j].D2R == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].D2R : TblMidiCh[MidiCh].CCValue.Op[j].D2R;
		outData->Op[j].RR	= (TblMidiCh[MidiCh].CCValue.Op[j].RR == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].RR : TblMidiCh[MidiCh].CCValue.Op[j].RR;
		outData->Op[j].KS	= (TblMidiCh[MidiCh].CCValue.Op[j].KS == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].KS : TblMidiCh[MidiCh].CCValue.Op[j].KS;
		outData->Op[j].DT1	= (TblMidiCh[MidiCh].CCValue.Op[j].DT1 == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].DT1 : TblMidiCh[MidiCh].CCValue.Op[j].DT1;
		outData->Op[j].MUL	= (TblMidiCh[MidiCh].CCValue.Op[j].MUL == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].MUL : TblMidiCh[MidiCh].CCValue.Op[j].MUL;
		outData->Op[j].DT2	= (TblMidiCh[MidiCh].CCValue.Op[j].DT2 == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].DT2 : TblMidiCh[MidiCh].CCValue.Op[j].DT2;
		outData->Op[j].F_AME= (TblMidiCh[MidiCh].CCValue.Op[j].F_AME == CCVALUE_NONSET)?VoTbl[VoNum].Op[j].F_AME : TblMidiCh[MidiCh].CCValue.Op[j].F_AME;
	}
	outData->SPD	= (TblMidiCh[MidiCh].CCValue.SPD == CCVALUE_NONSET)?VoTbl[VoNum].SPD : TblMidiCh[MidiCh].CCValue.SPD;
	outData->PMD	= (TblMidiCh[MidiCh].CCValue.PMD == CCVALUE_NONSET)?VoTbl[VoNum].PMD : TblMidiCh[MidiCh].CCValue.PMD;
	outData->AMD	= (TblMidiCh[MidiCh].CCValue.AMD == CCVALUE_NONSET)?VoTbl[VoNum].AMD : TblMidiCh[MidiCh].CCValue.AMD;
	outData->WF		= (TblMidiCh[MidiCh].CCValue.WF == CCVALUE_NONSET)?VoTbl[VoNum].WF : TblMidiCh[MidiCh].CCValue.WF;
	outData->NFQ	= (TblMidiCh[MidiCh].CCValue.NFQ == CCVALUE_NONSET)?VoTbl[VoNum].NFQ : TblMidiCh[MidiCh].CCValue.NFQ;
}

void OPMDRV::setCcFRQ_H(int MidiCh,int data)
{
	if ( TblMidiCh[MidiCh].CCValue.SPD == CCVALUE_NONSET ) {
		TblMidiCh[MidiCh].CCValue.SPD = 0;
	}
	int spd = TblMidiCh[MidiCh].CCValue.SPD & 0x01;
	spd |= (data & 0x7f) << 1;
	TblMidiCh[MidiCh].CCValue.SPD = spd;
	
	int spdSum = CalcLim8(spd, TblMidiCh[MidiCh].SPD );
	for(int ch=0;ch<CHMAX;ch++){
		if(MidiCh==TblCh[ch].MidiCh ){
			setSPD(ch,spdSum);
		}
	}
}

void OPMDRV::setCcFRQ_L(int MidiCh,int data)
{
	if ( TblMidiCh[MidiCh].CCValue.SPD == CCVALUE_NONSET ) {
		TblMidiCh[MidiCh].CCValue.SPD = 0;
	}
	int spd = TblMidiCh[MidiCh].CCValue.SPD & 0xfe;
	spd |= data & 0x01;
	TblMidiCh[MidiCh].CCValue.SPD = spd;
	
	int spdSum = CalcLim8(spd, TblMidiCh[MidiCh].SPD );
	for(int ch=0;ch<CHMAX;ch++){
		if(MidiCh==TblCh[ch].MidiCh ){
			setSPD(ch,spdSum);
		}
	}
}

void OPMDRV::setCcPMD(int MidiCh,int data)
{
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.PMD, 128);
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	for(int ch=0;ch<CHMAX;ch++){
		if(MidiCh==TblCh[ch].MidiCh ){
			setPMD(ch,ChData.PMD);
		}
	}
}

void OPMDRV::setCcAMD(int MidiCh,int data)
{
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.AMD, 128);
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	for(int ch=0;ch<CHMAX;ch++){
		if(MidiCh==TblCh[ch].MidiCh ){
			setAMD(ch,ChData.AMD);
		}
	}
}

void OPMDRV::setCcWF(int MidiCh,int data)
{
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.WF, 4);
	for(int ch=0;ch<CHMAX;ch++){
		if(MidiCh==TblCh[ch].MidiCh ){
			SendLfo(ch);
		}
	}
}

void OPMDRV::setCcNFQ(int MidiCh,int data)
{
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.NFQ, 32);
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	for(int ch=0;ch<CHMAX;ch++){
		if(MidiCh==TblCh[ch].MidiCh ){
			_iocs_opmset(0x00,ch);
			_iocs_opmset(0x0F,(ChData.NFQ&0x1f)|(ChData.F_NE&0x80));
		}
	}
}

void OPMDRV::setCcNE(int MidiCh,int data)
{
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.F_NE, 2);
	if ( TblMidiCh[MidiCh].CCValue.F_NE != CCVALUE_NONSET ) {
		TblMidiCh[MidiCh].CCValue.F_NE <<= 7;
	}
	
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	for(int ch=0;ch<CHMAX;ch++){
		if(MidiCh==TblCh[ch].MidiCh ){
			_iocs_opmset(0x00,ch);
			_iocs_opmset(0x0F,(ChData.NFQ&0x1f)|(ChData.F_NE&0x80));
		}
	}
}

void OPMDRV::setCcPBRange(int MidiCh, int data)
{
	if(data<=0 || data>=96){data=2;}
	TblMidiCh[MidiCh].BendRang=data;
}

void OPMDRV::setCcCON(int MidiCh,int data)
{
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.CON, 8);
	PanCng(MidiCh, TblMidiCh[MidiCh].Pan);
}

void OPMDRV::setCcFL(int MidiCh,int data)
{
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.FL, 8);
	PanCng(MidiCh, TblMidiCh[MidiCh].Pan);
}

void OPMDRV::setCcTL(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].TL, 128, true);
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);

	int Tl=CalcLimTL(ChData.Op[OpNum].TL,TblMidiCh[MidiCh].OpTL[OpNum]);
	for(int ch=0;ch<CHMAX;ch++){
		if(MidiCh==TblCh[ch].MidiCh ){
			setTL(ch,Tl,OpNum);
		}
	}
}

void OPMDRV::setCcMUL(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].MUL, 16);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0x40+addr,(pOp->DT1<<4)|(pOp->MUL&0x0f));	//DT1/MUL
		}
	}
}

void OPMDRV::setCcDT1(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].DT1, 8);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0x40+addr,(pOp->DT1<<4)|(pOp->MUL&0x0f));	//DT1/MUL
		}
	}
}

void OPMDRV::setCcDT2(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].DT2, 4);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0xC0+addr,(pOp->DT2<<6)|(pOp->D2R&0x1f));	 //DT2/D2R
		}
	}
}

void OPMDRV::setCcKS(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].KS, 4);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0x80+addr,(pOp->KS<<6)|(pOp->AR&0x1f));	 //KS/AR
		}
	}
}

void OPMDRV::setCcAR(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].AR, 32, true);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0x80+addr,(pOp->KS<<6)|(pOp->AR&0x1f));	 //KS/AR
		}
	}
}

void OPMDRV::setCcD1R(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].D1R, 32, true);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0xA0+addr,(pOp->F_AME)|(pOp->D1R&0x1f)); //AME/D1R
		}
	}
}

void OPMDRV::setCcD2R(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].D2R, 32, true);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0xC0+addr,(pOp->DT2<<6)|(pOp->D2R&0x1f));	 //DT2/D2R
		}
	}
}

void OPMDRV::setCcD1L(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].D1L, 16, true);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0xE0+addr,(pOp->D1L<<4)|(pOp->RR&0x0f));	//D1L/RR
		}
	}
}

void OPMDRV::setCcRR(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].RR, 16, true);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0xE0+addr,(pOp->D1L<<4)|(pOp->RR&0x0f));	//D1L/RR
		}
	}
}

void OPMDRV::setCcAME(int MidiCh,int data,int OpNum)
{
	OpNum&=0x03;
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.Op[OpNum].F_AME, 2);
	if ( TblMidiCh[MidiCh].CCValue.Op[OpNum].F_AME != CCVALUE_NONSET ) {
		TblMidiCh[MidiCh].CCValue.Op[OpNum].F_AME <<= 7;
	}
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	OPDATA *pOp = &(ChData.Op[OpNum]);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			int	addr = (OpNum<<3)+ch;
			_iocs_opmset(0xA0+addr,(pOp->F_AME)|(pOp->D1R&0x1f)); //AME/D1R
		}
	}
}


void OPMDRV::setCcPMS(int MidiCh,int data)
{
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.PMS, 8);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			_iocs_opmset(0x00,ch);
			_iocs_opmset(0x38+ch,(ChData.PMS<<4)|(ChData.AMS&0x3));//PMS/AMS
		}
	}
}

void OPMDRV::setCcAMS(int MidiCh,int data)
{
	setCcValue(MidiCh, data, TblMidiCh[MidiCh].CCValue.AMS, 4);
	
	//MIDIチャンネル情報から、発音中のボイスを探し、設定する
	CHDATA	ChData;
	mergeControlChange(&ChData, TblMidiCh[MidiCh].VoNum, MidiCh);
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			_iocs_opmset(0x00,ch);
			_iocs_opmset(0x38+ch,(ChData.PMS<<4)|(ChData.AMS&0x3));//PMS/AMS
		}
	}
}

void OPMDRV::setCcLFOSync(int MidiCh,int data)
{
	if (data == 0 ) {
		TblMidiCh[MidiCh].LFOSync = false;
	}
	else {
		TblMidiCh[MidiCh].LFOSync = true;
	}
}

void OPMDRV::forceLFOReset(int MidiCh,int data)
{
	if ( IsRealMode() ) {
		_iocs_opmset(0x01,data==0 ? 0x02:0x00);
	}
	else {
		for(int ch=0;ch<CHMAX;ch++) {
			if(MidiCh==TblCh[ch].MidiCh) {
				_iocs_opmset(0x00,ch);
				_iocs_opmset(0x01,data==0 ? 0x02:0x00);
			}
		}
	}
}

void OPMDRV::setCcVelocitySens(int MidiCh, int data, int OpNum)
{
	TblMidiCh[MidiCh].OpVelSence[OpNum]	= data & 0x7f;
}

void OPMDRV::setChAllocMode(bool mode)
{
	mAlternatePolyMode = mode;
}

//--ソフトウェアLFO

void OPMDRV::setCcSwLFO1_Dest(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwLfo[0].Dest = data;
}

void OPMDRV::setCcSwLFO1_WF(int MidiCh,int data)
{
	data = (data + 0x04) & 0x1f;
	int wf = data & 0x03;
	int pms= (data>>2) & 0x07;
	int ams= (data>>2) & 0x0f;
	TblMidiCh[MidiCh].SwLfo[0].WF = wf;
	TblMidiCh[MidiCh].SwLfo[0].PMS= pms;
	TblMidiCh[MidiCh].SwLfo[0].AMS= ams;
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			TblCh[ch].SwLfo[0].SetWaveForm(wf);
			TblCh[ch].SwLfo[0].SetPMSAMS(ch, (pms<<4) | ams );
		}
	}
}

void OPMDRV::setCcSwLFO1_FRQ(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwLfo[0].FRQ = data;
	data <<= 1;
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			TblCh[ch].SwLfo[0].SetLFRQ(data);
		}
	}
}

void OPMDRV::setCcSwLFO1_Depth(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwLfo[0].PMD = data;
	TblMidiCh[MidiCh].SwLfo[0].AMD = data;
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			TblCh[ch].SwLfo[0].SetPMDAMD(data);
			TblCh[ch].SwLfo[0].SetPMDAMD(0x80|data);
		}
	}
}

void OPMDRV::setCcSwLFO1_Sync(int MidiCh,int data)
{
	TblMidiCh[MidiCh].SwLfo[0].Sync = (data == 0) ? false:true;
}

void OPMDRV::setCcSwLFO1_Delay(int MidiCh,int data)
{
	TblMidiCh[MidiCh].SwLfo[0].Delay = data;
}

void OPMDRV::setCcSwLFO2_Dest(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwLfo[1].Dest = data;
}

void OPMDRV::setCcSwLFO2_WF(int MidiCh,int data)
{
	data = (data + 0x04) & 0x1f;
	int wf = data & 0x03;
	int pms= (data>>2) & 0x07;
	int ams= (data>>2) & 0x0f;
	TblMidiCh[MidiCh].SwLfo[1].WF = wf;
	TblMidiCh[MidiCh].SwLfo[1].PMS= pms;
	TblMidiCh[MidiCh].SwLfo[1].AMS= ams;
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			TblCh[ch].SwLfo[1].SetWaveForm(wf);
			TblCh[ch].SwLfo[1].SetPMSAMS(ch, ams | (pms << 4) );
		}
	}
}

void OPMDRV::setCcSwLFO2_FRQ(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwLfo[1].FRQ = data;
	data <<= 1;
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			TblCh[ch].SwLfo[1].SetLFRQ(data);
		}
	}
}

void OPMDRV::setCcSwLFO2_Depth(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwLfo[1].PMD = data;
	TblMidiCh[MidiCh].SwLfo[1].AMD = data;
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			TblCh[ch].SwLfo[1].SetPMDAMD(data);
			TblCh[ch].SwLfo[1].SetPMDAMD(0x80|data);
		}
	}
}

void OPMDRV::setCcSwLFO2_Sync(int MidiCh,int data)
{
	TblMidiCh[MidiCh].SwLfo[1].Sync = (data == 0) ? false:true;
}

void OPMDRV::setCcSwLFO2_Delay(int MidiCh,int data)
{
	TblMidiCh[MidiCh].SwLfo[1].Delay = data;
}

//--ソフトウェアエンベロープ

void OPMDRV::setCcCurrentSwEnv(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].CurrentEnvSel = data;
}

void OPMDRV::setCcSwEnvDest(int MidiCh,int data, int ind)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwEnv[ind].Dest = data;
}

void OPMDRV::setCcSwEnvDepth(int MidiCh,int data, int ind)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwEnv[ind].Depth = data;
}

void OPMDRV::setCcSwEnvLoop(int MidiCh,int data, int ind)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwEnv[ind].LoopNum = data;
	
	//発音中のノートに反映
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			TblCh[ch].Env[ind].SetLoopNum(data);
		}
	}
}

void OPMDRV::setCcSwEnvValue(int MidiCh,int data,int stage, int ind)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwEnv[ind].Value[stage] = data;
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			setSwEnvValue(ch,data,stage,ind);
		}
	}
}

void OPMDRV::setSwEnvValue(int ch,int data,int stage, int ind)
{
	TblCh[ch].Env[ind].SetStageValue(stage, data);
}

void OPMDRV::setCcSwEnvTime(int MidiCh,int data,int stage, int ind)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].SwEnv[ind].Time[stage] = data;
	for(int ch=0;ch<CHMAX;ch++) {
		if(MidiCh==TblCh[ch].MidiCh) {
			setSwEnvTime(ch,data,stage,ind);
		}
	}
}

void OPMDRV::setSwEnvTime(int ch,int data,int stage, int ind)
{
	int time = (1000 * (data*data)) / (127*127);	//1=10ms
	TblCh[ch].Env[ind].SetStageTime(stage, time);
}

//--コンプレッサー--

void OPMDRV::setCcCompInputCh(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].CompInputCh = data;
	if ( data == 0x7f ) {
		//オフにする
		pOPM->setCompInputChBit(MidiCh, 0);
	}
	else {
		int beginTrack = data&0x0f;				//1-4ビット目で開始トラックを表す
		bool softKnee = (data&0x10)?true:false;	//5ビット目がonならソフトニーを表す
		int trackNum = (data>>5)&0x03;			//最後の2ビットで、続くトラック数を表す
		
		int chBit = 0;
		for ( int i=0; i<=trackNum; i++) {
			chBit |= 1<<((beginTrack+i)&0x0f);
		}
		pOPM->setCompInputChBit(MidiCh, chBit);
		pOPM->GetComp(MidiCh)->setSoftKnee(softKnee);
	}
}

void OPMDRV::setCcCompThreshold(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].CompThreshold = data;
	float gain = powf(2.0f, -data/16.0f)*32768.0f;
	pOPM->GetComp(MidiCh)->setThresh(gain);
	
	TblMidiCh[MidiCh].CompOutGain = calcCompOutGain(TblMidiCh[MidiCh].CompRatio, data);
}

void OPMDRV::setCcCompAtk(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].CompAtk = data;
	pOPM->GetComp(MidiCh)->setAR((data*data*1.0f)/(127*127));	//最大1.0s
}

void OPMDRV::setCcCompRatio(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].CompRatio = data;
	pOPM->GetComp(MidiCh)->setRatio((data*1.0f)/(127));
	
	TblMidiCh[MidiCh].CompOutGain = calcCompOutGain(data, TblMidiCh[MidiCh].CompThreshold);
}

void OPMDRV::setCcCompRel(int MidiCh,int data)
{
	data &= 0x7f;
	TblMidiCh[MidiCh].CompRel = data;
	pOPM->GetComp(MidiCh)->setRR((data*data*(10.0f/3.0f))/(127*127));	//最大3.33s
}

float OPMDRV::calcCompOutGain(int ratio, int threshold)
{
	float f_ratio = (ratio*1.0f)/(127);
	float f_thresh = powf(2.0f, -threshold/16.0f) * 32768.0f;
	float gain = (32768.0f - f_thresh) * (1.0f - f_ratio) + f_thresh;
	return (32768.0f/gain);
}

void OPMDRV::SetLocalOn(bool isOn)
{
	//実機動作でない場合は何もしない
	if ( IsRealMode() ) {
		pOPM->SetMute(isOn?false:true);
	}
}

void OPMDRV::ResetAllProgramParam(int MidiCh)
{
	TblMidiCh[MidiCh].CCValue.F_PAN	= CCVALUE_NONSET;
	TblMidiCh[MidiCh].CCValue.CON	= CCVALUE_NONSET;
	TblMidiCh[MidiCh].CCValue.FL	= CCVALUE_NONSET;
	TblMidiCh[MidiCh].CCValue.AMS	= CCVALUE_NONSET;
	TblMidiCh[MidiCh].CCValue.PMS	= CCVALUE_NONSET;
	TblMidiCh[MidiCh].CCValue.F_NE	= CCVALUE_NONSET;
	for ( int op=0; op<4; op++ ) {
		TblMidiCh[MidiCh].CCValue.Op[op].TL		= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].D1L	= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].AR		= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].D1R	= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].D2R	= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].RR		= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].KS		= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].DT1	= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].MUL	= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].DT2	= CCVALUE_NONSET;
		TblMidiCh[MidiCh].CCValue.Op[op].F_AME	= CCVALUE_NONSET;
		
		TblMidiCh[MidiCh].OpVelSence[op]		= CCVALUE_NONSET;
	}
	TblMidiCh[MidiCh].CCValue.SPD	= CCVALUE_NONSET;
	TblMidiCh[MidiCh].CCValue.PMD	= CCVALUE_NONSET;
	TblMidiCh[MidiCh].CCValue.AMD	= CCVALUE_NONSET;
	TblMidiCh[MidiCh].CCValue.WF	= CCVALUE_NONSET;
	TblMidiCh[MidiCh].CCValue.NFQ	= CCVALUE_NONSET;
}

void OPMDRV::ResetAllControllers(int MidiCh)
{
	TblMidiCh[MidiCh].Vol=127;
	TblMidiCh[MidiCh].Vol2=127;
	TblMidiCh[MidiCh].Pan=0x40;
	TblMidiCh[MidiCh].BendRang=2;
	TblMidiCh[MidiCh].SPD=0x4000;
	TblMidiCh[MidiCh].PMD=0x4000;
	TblMidiCh[MidiCh].AMD=0x4000;
	TblMidiCh[MidiCh].BfNote=0xff;
	TblMidiCh[MidiCh].RPN=0;
	TblMidiCh[MidiCh].NRPN=0;
	TblMidiCh[MidiCh].CuRPN=false;
	TblMidiCh[MidiCh].PortaSpeed=0;
	TblMidiCh[MidiCh].PortaOnOff=false;
	TblMidiCh[MidiCh].LFOdelay=0;
	TblMidiCh[MidiCh].LFOSync=true;
	TblMidiCh[MidiCh].OpTL[0]=0x4000;
	TblMidiCh[MidiCh].OpTL[1]=0x4000;
	TblMidiCh[MidiCh].OpTL[2]=0x4000;
	TblMidiCh[MidiCh].OpTL[3]=0x4000;
	
	ResetAllProgramParam(MidiCh);
	
	for ( int i=0; i<SWLFO_NUM; i++ ) {
		TblMidiCh[MidiCh].SwLfo[i].Dest=SWENV_DEST_AMP;
		TblMidiCh[MidiCh].SwLfo[i].WF=0;
		TblMidiCh[MidiCh].SwLfo[i].PMS=1;
		TblMidiCh[MidiCh].SwLfo[i].AMS=1;
		TblMidiCh[MidiCh].SwLfo[i].FRQ=96;
		TblMidiCh[MidiCh].SwLfo[i].PMD=0;
		TblMidiCh[MidiCh].SwLfo[i].AMD=0;
		TblMidiCh[MidiCh].SwLfo[i].Sync=true;
		TblMidiCh[MidiCh].SwLfo[i].Delay=0;
	}
	TblMidiCh[MidiCh].SwLfo[0].Dest=SWENV_DEST_PITCH;
	
	for ( int i=0; i<SWENV_NUM; i++ ) {
		TblMidiCh[MidiCh].SwEnv[i].Dest=SWENV_DEST_AMP;
		TblMidiCh[MidiCh].SwEnv[i].Depth=0;
		TblMidiCh[MidiCh].SwEnv[i].Value[0]=0;
		TblMidiCh[MidiCh].SwEnv[i].Time[0]=0;
		TblMidiCh[MidiCh].SwEnv[i].Value[1]=0;
		TblMidiCh[MidiCh].SwEnv[i].Time[1]=0;
		TblMidiCh[MidiCh].SwEnv[i].Value[2]=0;
		TblMidiCh[MidiCh].SwEnv[i].Time[2]=0;
		TblMidiCh[MidiCh].SwEnv[i].Value[3]=0;
		TblMidiCh[MidiCh].SwEnv[i].Time[3]=0;
		TblMidiCh[MidiCh].SwEnv[i].Value[4]=0;
		TblMidiCh[MidiCh].SwEnv[i].Time[4]=0;
	}
	
	TblMidiCh[MidiCh].AutoPan.Enable = false;
	TblMidiCh[MidiCh].AutoPan.StepTime = 0.5;
	TblMidiCh[MidiCh].AutoPan.Table[0] = 0;
	TblMidiCh[MidiCh].AutoPan.Table[1] = 64;
	TblMidiCh[MidiCh].AutoPan.Table[2] = 127;
	TblMidiCh[MidiCh].AutoPan.Table[3] = 64;
	TblMidiCh[MidiCh].AutoPan.Length = 4;
	TblMidiCh[MidiCh].AutoPan.Delay = 50;
	
	TblMidiCh[MidiCh].CompInputCh=0x7f;	//オフ状態
	TblMidiCh[MidiCh].CompThreshold=0;
	TblMidiCh[MidiCh].CompAtk=20;
	TblMidiCh[MidiCh].CompRatio=90;
	TblMidiCh[MidiCh].CompRel=40;
	setCcCompInputCh(MidiCh,TblMidiCh[MidiCh].CompInputCh);
	setCcCompThreshold(MidiCh,TblMidiCh[MidiCh].CompThreshold);
	setCcCompAtk(MidiCh,TblMidiCh[MidiCh].CompAtk);
	setCcCompRatio(MidiCh,TblMidiCh[MidiCh].CompRatio);
	setCcCompRel(MidiCh,TblMidiCh[MidiCh].CompRel);
	
	TblMidiCh[MidiCh].CurrentEnvSel = 0;
	TblMidiCh[MidiCh].CcDirection = CCDIRECTION_NATURAL;
}