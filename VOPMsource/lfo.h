#define N_CH 8
#define	SIZELFOTBL	512				// 2^9
#define	SIZELFOTBL_BITS	9
#define	LFOPRECISION	4096	// 2^12

#ifndef __CLASSLFO__
class Lfo {
private:
	int Pmsmul[N_CH];	// 0, 1, 2, 4, 8, 16, 32, 32
	int Pmsshl[N_CH];	// 0, 0, 0, 0, 0,  0,  1,  2
	int	Ams[N_CH];	// ���V�t�g�� 31(0), 0(1), 1(2), 2(3)
	int	PmdPmsmul[N_CH];	// Pmd*Pmsmul[]
	int	Pmd;
	int	Amd;

	int LfoStartingFlag;	// 0:LFO��~��  1:LFO���쒆
	int	LfoOverFlow;	// LFO t�̃I�[�o�[�t���[�l
	int	LfoTime;	// LFO��p t
	int	LfoTimeAdd;	// LFO��p��t
	int LfoIdx;	// LFO�e�[�u���ւ̃C���f�b�N�X�l
	int LfoSmallCounter;	// LFO�����������J�E���^ (0�`15�̒l���Ƃ�)
	int LfoSmallCounterStep;	// LFO�����������J�E���^�p�X�e�b�v�l (16�`31)
	int	Lfrq;		// LFO���g���ݒ�l LFRQ
	int	LfoWaveForm;	// LFO wave form

	int	PmTblValue,AmTblValue;
	int	PmValue[N_CH],AmValue[N_CH];

	char	PmTbl0[SIZELFOTBL], PmTbl2[SIZELFOTBL];
	unsigned char	AmTbl0[SIZELFOTBL], AmTbl2[SIZELFOTBL];

	 void CulcTblValue();
	 void	CulcPmValue(int ch);
	 void	CulcAmValue(int ch);
	 void CulcAllPmValue();
	 void CulcAllAmValue();

public:
	Lfo(void);
	~Lfo() {};

	 void Init();
	 void InitSamprate();

	 void LfoReset();
	 void LfoStart();
	 void SetLFRQ(int n);
	 void SetPMDAMD(int n);
	 void	SetWaveForm(int n);
	 void	SetPMSAMS(int ch, int n);

	 void	Update();
	 int	GetPmValue(int ch);
	 int	GetAmValue(int ch);
};
#define __CLASSLFO__
#endif
