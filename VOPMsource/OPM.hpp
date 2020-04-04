//opm.hpp
#include "op.h"
#include "lfo.h"

#include "Compressor.h"

#define	CMNDBUFSIZE	65535
#define	FlagBufSize	7

#define	OPMLPF_COL	56

class Opm {

	//	short	PcmBuf[PCMBUFSIZE][2];
	//short	(*PcmBuf)[2];
public:
	unsigned int PcmBufSize;
	volatile unsigned int PcmBufPtr;
	int	TotalVolume;	// ���� x/256
private:



	unsigned int	TimerID;
	char *Author;

	Op	op[8][4];	// �I�y���[�^0�`31
	int	EnvCounter1;	// �G���x���[�v�p�J�E���^1 (0,1,2,3,4,5,6,...)
	int	EnvCounter2;	// �G���x���[�v�p�J�E���^2 (3,2,1,3,2,1,3,2,...)
	int	pan[2][N_CH];	// 0:���� -1:�o��
	int	CPan[N_CH];
	int	CVol[N_CH];
	int CVel[N_CH];
	Lfo	lfo[N_CH];
	int	SLOTTBL[8*4];
	int	CuCh;					//Current Channel Reg�g���Ɏg�p

	unsigned char	CmndBuf[CMNDBUFSIZE+1][2];
	unsigned int	CmndBufPoint[CMNDBUFSIZE+1];


	volatile int	NumCmnd;
	int	CmndReadIdx,CmndWriteIdx;
	int CmndRate;

	inline void SetConnection(int ch, int alg);
	volatile int	OpOut[8];
	int	OpOutDummy;
	
	double inpopmbuf_dummy;
	short InpOpmBuf0[OPMLPF_COL*2],InpOpmBuf1[OPMLPF_COL*2];
	int InpOpm_idx;
	int OpmLPFidx; short *OpmLPFp;
	double inpadpcmbuf_dummy;

	int	OutOpm[2];
	int InpInpOpm[2],InpOpm[2];
	int	InpInpOpm_prev[2],InpOpm_prev[2];
	int	InpInpOpm_prev2[2],InpOpm_prev2[2];
	int OpmHpfInp[2], OpmHpfInp_prev[2], OpmHpfOut[2];
	int OutInpAdpcm[2],OutInpAdpcm_prev[2],OutInpAdpcm_prev2[2],
		OutOutAdpcm[2],OutOutAdpcm_prev[2],OutOutAdpcm_prev2[2];	// �����t�B���^�[�Q�p�o�b�t�@

	volatile unsigned char	PpiReg;


	unsigned char	OpmRegNo;		// ���ݎw�肳��Ă���OPM���W�X�^�ԍ�
	unsigned char	OpmRegNo_backup;		// �o�b�N�A�b�v�pOPM���W�X�^�ԍ�

	int Dousa_mode;		// 0:�񓮍� 1:X68Sound_Start��  2:X68Sound_PcmStart��

	//�ǉ� 2007.11.22 sam
	//�o�C�A�X�@�\�ǉ� �o�͂̍ŏI�i�ł̉E�V�t�g��
	int Bias;
	//�ǉ� 2007.11.22 sam
	//LPF�p�X�@�\�ǉ� LPF�t���O 0:Pass 1:Enable
	int EnableLPF;

//�R�[���o�b�N�@�\�ǉ��@2003.10.05 sam
	void (*CallBackFunc)(int sampleframes);	//CallBack�֐��|�C���^

//public�@�s�v�֐��폜�@2002.3.10�@sam
public:
	Opm(void);
	~Opm();

	//inline
	void pcmset22(int,float**,bool);
	inline void MakeTable();
	inline int Start(int samprate, int opmflag, int adpcmflag,
					int betw, int pcmbuf, int late, double rev);
	//inline
	int StartPcm(int samprate, int opmflag, int adpcmflag, int pcmbuf);
	//inline
	int SetSamprate(int samprate);
	int SetOpmClock(int clock);
	//inline int WaveAndTimerStart();
	inline int SetOpmWait(int wait);
	inline void CulcCmndRate();
	inline void Reset();
	inline void ResetSamprate();
	inline void Free();

	inline unsigned char OpmPeek();
	//inline void OpmReg(unsigned char no);
	//inline
	void OpmPoke(unsigned char,unsigned char,unsigned int);
	inline void ExecuteCmnd(unsigned int);
	void ForceNoteOff(void);


	//inline
	int SetTotalVolume(int v);

	inline void PushRegs();
	inline void PopRegs();
	
	//�ǉ� osoumen
private:
	float		CompOutAmp[16];		//MIDI�`�����l���R���v�o�͒l
	int			OpOutMidiCh[8];		//�o��MIDI�`�����l��
	int			ChBits[16];			//�g���b�N��\���r�b�g�l���R���X�g���N�^�ō쐬
	int			CompInputChBit[16];	//����MIDI�`�����l���r�b�g��
	Compressor	Comp[16];
	bool		IsMute;				//�����o�͂��Ȃ�
public:
	void SetOutMidiCh(int slot, int MidiCh) { OpOutMidiCh[slot] = MidiCh; }
	float GetOutAmp(int MidiCh) { return CompOutAmp[MidiCh]; }
	void setCompInputChBit(int MidiCh, int chBit) { CompInputChBit[MidiCh] = chBit; }
	Compressor* GetComp(int MidiCh) { return &Comp[MidiCh]; }
	void SetMute(bool isMute) { IsMute = isMute; }
};
