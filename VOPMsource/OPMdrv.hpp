#ifndef __OPMDRV__
#include <list>

#if AU
#include "audioeffectx_intf.h"
#else
#include "aeffectx.h"
#endif

#include "GmcInterface.h"
#include "S98File.h"
#include "lfo.h"

#include "Compressor.h"

using namespace std;

#define CHMAX 8
#define VOMAX 128

#define ERR_NOERR 0x0000
#define ERR_NOFILE 0x10000000
#define ERR_SYNTAX 0x20000000
#define ERR_LESPRM 0x30000000

#define CHSLOT8 15 //Midi 15Ch assigned OPM8ch

#define SWLFO_NUM 2
#define SWENV_NUM 128
#define SWENV_STAGES 5

class Envelope {
public:
	Envelope()
	: mStage(1)
	, mStagePos(0)
	, mValue(0)
	, mPrevStageValue(0)
	, mLoopNum(0)
	, mLoopCount(0)
	, mTime(0)
	{
		for ( int i=0; i<=SWENV_STAGES; i++ ) {
			mStageValue[i] = 0;
			mStageTime[i] = 0;
		}
	}
	void	SetStageValue(int stage, int value)
	{
		if ( stage < SWENV_STAGES && stage >= 0 ) {
			mStageValue[stage] = value;
		}
	}
	void	SetStageTime(int stage, int value)
	{
		if ( stage < SWENV_STAGES && stage >= 0 ) {
			mStageTime[stage] = value;
		}
	}
	void	SetLoopNum(int loopNum)
	{
		mLoopNum = loopNum;
	}
	int	GetEnvValue()
	{
		CalcValue();
		return mValue;
	}
	void	CalcValue()
	{
		if ( mStageTime[mStage] == 0 ) {
			mValue = mStageValue[mStage];
		}
		else {
			mValue = (mStageTime[mStage] - mStagePos)*mPrevStageValue + mStagePos*mStageValue[mStage];
			mValue *= 8;
			mValue /= mStageTime[mStage];
		}
	}
	void	Reset()
	{
		mStage = 0;
		mStagePos = 0;
		mValue = 0;
		mPrevStageValue = 0;
		mLoopCount = 0;
		mTime = 0;
	}
	void	KeyOn()
	{
		mPrevStageValue = mValue/8;
		mStage = 0;
		mStagePos = 0;
		mLoopCount = 0;
		mTime = 0;
	}
	void	KeyOff()
	{
		mPrevStageValue = mValue/8;
		mStage = SWENV_STAGES-1;
		mStagePos = 0;
	}
	void	Update()
	{
		CalcValue();
		mStagePos++;
		if ( mStagePos > mStageTime[mStage] ) {
			mStagePos = 0;
			mPrevStageValue = mStageValue[mStage];
			mStage++;
			if ( mStage == SWENV_STAGES-1 ) {	//終端まで来た
				if ( (mLoopCount < mLoopNum) || (mLoopNum >= 127) ) {
					mStage = 0;
					mLoopCount++;
				}
				else {
					mStage = SWENV_STAGES-2;
					mStagePos = mStageTime[mStage];
				}
			}
			else if ( mStage == SWENV_STAGES ) {
				mStage = SWENV_STAGES-1;
				mStagePos = mStageTime[mStage];
			}
		}
		mTime++;
	}
	int GetTime() { return mTime; }
private:
	int		mStage;
	int		mStagePos;
	int		mValue;
	int		mPrevStageValue;
	int		mLoopNum;
	int		mLoopCount;
	int		mStageValue[SWENV_STAGES];
	int		mStageTime[SWENV_STAGES];
	int		mTime;
};


class OPDATA{
	public:
	 //EG-Level
		unsigned int TL;
		unsigned int D1L;
	 //EG-ADR
		unsigned int AR;
		unsigned int D1R;
		unsigned int D2R;
		unsigned int RR;
	 //DeTune
		unsigned int KS;
		unsigned int DT1;
		unsigned int MUL;
		unsigned int DT2;
		unsigned int F_AME;

		OPDATA(void);
};

class CHDATA{
	public:
		unsigned int F_PAN;//L:0x80Msk R:0x40Msk
		unsigned int CON;
		unsigned int FL;
		unsigned int AMS;
		unsigned int PMS;
		unsigned int F_NE;
		unsigned int OpMsk;
		class OPDATA Op[4];
		char Name[16];
		unsigned int SPD;
		unsigned int PMD;
		unsigned int AMD;
		unsigned int WF;
		unsigned int NFQ;
		CHDATA(void);
// ~CHDATA(void);

};


struct SWLFOParam {
	int Dest;
	int	WF;
	int	PMS;
	int	AMS;
	int FRQ;
	int	PMD;
	int	AMD;
	bool Sync;
	int	Delay;
};

struct SWENVParam {
	int Dest;
	int Depth;
	int LoopNum;
	int Time[SWENV_STAGES];
	int Value[SWENV_STAGES];
};

struct AUTOPANParam {
	bool	Enable;
	double	StepTime;
	int		Table[8];
	int		Length;
	int		Delay;
};

struct TBLMIDICH{
	int VoNum;				//音色番号 0-
	int Bend;				//ピッチベンド -8192～+8191[signed 14bit]
	int Vol;				//ボリューム 0-127
	int Pan;				//L:0x80 Msk,R:0x40 Msk
	int BendRang;			//ピッチベンドレンジ　1-??[半音の倍率]
	int PMD;
	int AMD;
	int SPD;				//SPDの相対変化量
	int OpTL[4];			//OpTLの相対変化量
	unsigned int RPN;
	unsigned int NRPN;
	bool CuRPN;
	int BfNote;				//前回のノート番号（ポルタメント用）
	int PortaSpeed;			//ポルタメントスピード
	bool PortaOnOff;
	int LFOdelay;
	
	//追加by osoumen
	int Vol2;				//エクスプレッション 0-127
	bool LFOSync;			//キーオン時にHWLFOの位相リセットを行うか(デフォルトはON)
	int	OpVelSence[4];		//オペレータ毎のベロシティ感度
	CHDATA CCValue;			//コントロールチェンジで変更した音色パラメータ
	int	CcMode;				// 0-ORIGINAL Mode 1-EXPAND Mode
	int	CurrentEnvSel;		//操作しているソフトエンベロープ 0-127
	int CcDirection;		//コントロールチェンジの変化方向 0-Natural 1-Register
	
	SWLFOParam		SwLfo[SWLFO_NUM];
	SWENVParam		SwEnv[SWENV_NUM];
	AUTOPANParam	AutoPan;
	
	int	CompInputCh;	//0x7f=off
	int	CompThreshold;
	int	CompAtk;
	int	CompRatio;
	int	CompRel;
	float CompOutGain;	//自動音量補正ゲイン
};





class OPMDRV{
// friend class VOPM;
private:
	unsigned int SampleTime;	//HOSTから指定されたDelta時間
	char CuProgName[64];		//現在HOSTに選択されているプログラム名
	int CuProgNum;				//現在HOSTに選択されているプログラム番号

	int NoisePrm;				//OPM ノイズパラメータ
	int MstVol;					//マスターボリューム
	bool PolyMono;				//Poly true,Mono false
	class Opm *pOPM;
	int OpmClock;				//OPMのクロック 0:3.58MHz 1:4MHz
//発音CH管理双方向リスト
	list<int> PlayCh,WaitCh;
	
	struct {
		int VoNum;				//音色番号0-
		int MidiCh;				//Midiチャンネル
		int Note;				//ノート番号(0-127,未使用時255)
		int CopyOfNote;			//キーオフ後のピッチベンドで使用
		int Vel;				//ベロシティ
		int PortaBend;			//ポルタメント用のベンドオフセット
		int PortaSpeed;
		int LFOdelayCnt;
		
		//ソフトウェアLFO
		Lfo			SwLfo[SWLFO_NUM];
		int			SwLfoDelayCnt[SWLFO_NUM];
		//ソフトウェアエンベロープ
		Envelope	Env[SWENV_NUM];
		//コンプリダクション量
		float		reduction;
		//オートパン
		bool		AutoPanEnable;
		int			AutoPanDelayCnt;
	}TblCh[CHMAX];

	struct TBLMIDICH TblMidiCh[16];	//Midiチャンネル設定テーブル

	int AtachCh(void);			//チャンネル追加
	void DelCh(int);			//チャンネル削除
	int ChkEnCh(void);			//空きチャンネルを返す。
	int ChkSonCh(int,int);		//MidiChとベロシティより発音中のチャンネルを探す。
	int NonCh(int,int,int,int);	//MidiChとノート、ベロシティで発音する。スロットの指定も可。
	int NoffCh(int,int);		//MidiChとノートで消音する。

	void SendVo(int);			//音色パラメータをOPMに送る
	void SendKc(int);			//音階データをOPMにセットする
	void SendTl(int);			//チャンネル音量をセットする
	void SendPan(int,int);		//Panをセットする
	void SendLfo(int);


public:
	OPMDRV(class Opm *);				//コンストラクタ

	class CHDATA VoTbl[VOMAX];	//音色テーブル
															//SLOT 1音毎の設定クラス
	MidiProgramName MidiProg[16];
	void setDelta(unsigned int);						//コマンドDelta時間設定
	void NoteOn(int Ch,int Note,int Vol);		//ノートＯＮ
	void NoteOff(int Ch,int Note);					//ノートＯＦＦ
	void NoteAllOff(int Ch);								//全ノートＯＦＦ
	void ForceAllOff(int Ch);								//強制発音停止
	void ForceAllOff();											//強制発音停止(全チャンネル対応)
	void BendCng(int Ch,int Bend);					//ピッチベンド変化
	void VolCng(int Ch,int Vol);						//ボリューム変化
	void Vol2Cng(int Ch,int Vol);						//ボリューム変化
	void VoCng(int Ch,int VoNum);						//音色チェンジ
	void MsVolCng(int MVol);								//マスターボリューム変化
	void PanCng(int Ch,int Pan);						//Pan変化
	int Load(const char *);												//音色、LFO設定ロード
	int Save(char *);												//音色、LFO設定セーブ
	int getProgNum(void);										//音色ナンバー取得
	void setProgNum(long );									//音色ナンバーセット
	void setCuProgNum(long ProgNum);
	void setProgName(char* );								//音色名セット
	char* getProgName(void);								//音色名取得
	char* getProgName(int);									//音色名取得
	unsigned char getPrm(int);							//index(Vst kNum)によるパラメータセット
	void setPrm(int,unsigned char);					//index(Vst kNum)によるパラメータ取得
	void setRPNH(int,int);
	void setNRPNH(int,int);
	void setRPNL(int,int);
	void setNRPNL(int,int);
	void setData(int,int);
	void setPoly(bool);
	void AutoSeq(int sampleframes);
	void setPortaSpeed(int MidiCh,int speed);
	void setPortaOnOff(int MidiCh,bool onoff);
	void setPortaCtr(int MidiCh,int Note);
	void setLFOdelay(int MidiCh,int data);

	~OPMDRV(void);									//デストラクタ
	long getMidiProgram(long channel,MidiProgramName *curProg);
	void ChDelay(int );


	void setCcTL_L(int MidiCh,int data,int OpNum);
	void setCcTL_H(int MidiCh,int data,int OpNum);

	void setCcLFQ_L(int MidiCh,int data);
	void setCcLFQ_H(int MidiCh,int data);

	void setCcPMD_L(int MidiCh,int data);
	void setCcPMD_H(int MidiCh,int data);

	void setCcAMD_L(int MidiCh,int data);
	void setCcAMD_H(int MidiCh,int data);

private:
	int CalcLim8(int VoData,int DifData);
	int CalcLim7(int VoData,int DifData);
	int CalcLimTL(int VoData,int DifData);

	void setAMD(int ch,int Amd);
	void setPMD(int ch,int Pmd);
	void setSPD(int ch,int Spd);
	
	float ApplyVelocitySens( float vel, int velSens );
	float CalcTLFromAmp( float amp );
	int calcTLAdjust(int ch, int OpNum);
	void setTL(int ch,int Tl,int OpNum);
	
	//GIMIC制御
public:
	void OpenDevices();
	void CloseDevices();
	void FrameTask(int frameSamples);
private:
	void _iocs_opmset(unsigned char RegNo,unsigned char data);
	bool IsRealMode();
	GmcInterface	*m_pRealChip;
	
	//S98ログ
public:
	void BeginS98Log();
	void MarkS98Loop();
	void EndS98Log();
	int SaveS98Log(const char *path);
	bool CanSaveS98();
private:
	S98File		*m_pRegDumper;
	bool		mIsDumperRunning;
	int			mFrameStartPos;
	double		mTickPerSec;	//S98ログ開始時にテンポより算出して設定される
	
	//拡張モード
public:
	//処理した場合trueを、処理されなかった場合falseを返す
	bool	HandleControlChange( int MidiCh, int CCNum, int value );
	
	void	setCcValue(int MidiCh, int data, unsigned int &CcValue, unsigned range, bool reverse=false);
	void	mergeControlChange( CHDATA *outData, int VoNum, int MidiCh );
	void	setCcFRQ_H(int MidiCh,int data);
	void	setCcFRQ_L(int MidiCh,int data);
	void	setCcPMD(int MidiCh,int data);
	void	setCcAMD(int MidiCh,int data);
	void	setCcWF(int MidiCh,int data);
	void	setCcNFQ(int MidiCh,int data);
	void	setCcNE(int MidiCh,int data);
	
	void	setCcPBRange(int MidiCh, int data);
	
	void	setCcCON(int MidiCh,int data);
	void	setCcFL(int MidiCh,int data);
	void	setCcTL(int MidiCh,int data,int OpNum);
	void	setCcMUL(int MidiCh,int data,int OpNum);
	void	setCcDT1(int MidiCh,int data,int OpNum);
	void	setCcDT2(int MidiCh,int data,int OpNum);
	void	setCcKS(int MidiCh,int data,int OpNum);
	void	setCcAR(int MidiCh,int data,int OpNum);
	void	setCcD1R(int MidiCh,int data,int OpNum);
	void	setCcD2R(int MidiCh,int data,int OpNum);
	void	setCcD1L(int MidiCh,int data,int OpNum);
	void	setCcRR(int MidiCh,int data,int OpNum);
	void	setCcAME(int MidiCh,int data,int OpNum);
	void	setCcPMS(int MidiCh,int data);
	void	setCcAMS(int MidiCh,int data);
	
	void	setCcLFOSync(int MidiCh,int data);
	void	forceLFOReset(int MidiCh, int data);
	void	setCcVelocitySens(int MidiCh, int data,int OpNum);
	void	setChAllocMode(bool mode);
	
	void	setCcSwLFO1_Dest(int MidiCh,int data);
	void	setCcSwLFO1_WF(int MidiCh,int data);
	void	setCcSwLFO1_FRQ(int MidiCh,int data);
	void	setCcSwLFO1_Depth(int MidiCh,int data);
	void	setCcSwLFO1_Sync(int MidiCh,int data);
	void	setCcSwLFO1_Delay(int MidiCh,int data);
	void	setCcSwLFO2_Dest(int MidiCh,int data);
	void	setCcSwLFO2_WF(int MidiCh,int data);
	void	setCcSwLFO2_FRQ(int MidiCh,int data);
	void	setCcSwLFO2_Depth(int MidiCh,int data);
	void	setCcSwLFO2_Sync(int MidiCh,int data);
	void	setCcSwLFO2_Delay(int MidiCh,int data);

	void	setCcCurrentSwEnv(int MidiCh,int data);
	void	setCcSwEnvDest(int MidiCh,int data, int ind);
	void	setCcSwEnvDepth(int MidiCh,int data, int ind);
	void	setCcSwEnvLoop(int MidiCh,int value, int ind);
	void	setCcSwEnvValue(int MidiCh,int data,int stage, int ind);
	void	setCcSwEnvTime(int MidiCh,int data,int stage, int ind);
private:
	void	setSwEnvValue(int ch,int data,int stage, int ind);
	void	setSwEnvTime(int ch,int data,int stage, int ind);

public:
	void	setCcCompInputCh(int MidiCh,int data);
	void	setCcCompThreshold(int MidiCh,int data);
	void	setCcCompAtk(int MidiCh,int data);
	void	setCcCompRatio(int MidiCh,int data);
	void	setCcCompRel(int MidiCh,int data);
	float	calcCompOutGain(int ratio, int threshold);

	void	ResetAllProgramParam(int MidiCh);
	void	ResetAllControllers(int MidiCh);
	void	SetLocalOn(bool isOn);
private:
	static const int	CCMODE_ORIGINAL = 0;
	static const int	CCMODE_EXPAND = 1;
	static const int	CCDIRECTION_NATURAL = 0;
	static const int	CCDIRECTION_REGISTER = 1;
	static const int	SWENV_DEST_AMP = 1;
	static const int	SWENV_DEST_PITCH = 2;
	static const int	SWENV_DEST_OP1 = 4;
	static const int	SWENV_DEST_OP2 = 8;
	static const int	SWENV_DEST_OP3 = 16;
	static const int	SWENV_DEST_OP4 = 32;
	static const int	SWENV_DEST_LFOFRQ = 64;
	static const unsigned int	CCVALUE_NONSET = 0xffffffff;
	float	log_2_inv;			//ボリューム->TL計算用
	bool	mAlternatePolyMode;
	
	double	mTempo;
	double	mBeatPos;
public:
	void	setTempo(double tempo) { mTempo = tempo; }
	void	setBeatPos(double beatPos) { mBeatPos = beatPos; }
};
#define __OPMDRV__
#endif
