#define	PI	3.1415926535897932
#define	KEYON	-1
#define	ATACK	0
#define	DECAY	1
#define	SUSTAIN	2
#define	SUSTAIN_MAX	3
#define	RELEASE	4
#define	RELEASE_MAX	5

#define CULC_DELTA_T	(0x7FFFFFFF)
#define	CULC_ALPHA		(0x7FFFFFFF)

#ifndef __CLASSOP__
class Op {
public:
	volatile int	inp;			// FM�ϒ��̓���
private:
	int LfoPitch;	// �O���lfopitch�l, CULC_DELTA_T�l�̎���DeltaT���Čv�Z����B
	int	T;		// ���ݎ��� (0 <= T < SIZESINTBL*PRECISION)
	int DeltaT;	// ��t
	int	Ame;		// 0(�g�������������Ȃ�), -1(�g��������������)
	int	LfoLevel;	// �O���lfopitch&Ame�l, CULC_ALPHA�l�̎���Alpha���Čv�Z����B
	int	Alpha;	// �ŏI�I�ȃG���x���[�v�o�͒l

//�ǉ� 2006.03.26 sam Lfo�̍X�V��Sin�e�[�u����0�N���X���ɏC�����邽��
	bool	LfoLevelReCalc;
	short	SinBf;

public:
	volatile int	*out;			// �I�y���[�^�̏o�͐�
	volatile int	*out2;			// �I�y���[�^�̏o�͐�(alg=5����M1�p)
	volatile int	*out3;			// �I�y���[�^�̏o�͐�(alg=5����M1�p)
private:
	int	Pitch;	// 0<=pitch<10*12*64
	int	Dt1Pitch;	// Step �ɑ΂���␳��
	int	Mul;	// 0.5*2 1*2 2*2 3*2 ... 15*2
	int	Tl;		// (128-TL)*8

	int	Out2Fb;	// �t�B�[�h�o�b�N�ւ̏o�͒l
	int	Inp_last;	// �Ō�̓��͒l
	int	Fl;		// �t�B�[�h�o�b�N���x���̃V�t�g�l(31,7,6,5,4,3,2,1)
	int	ArTime;	// AR��p t

	int	NoiseCounter;	// Noise�p�J�E���^
	int NoiseStep;	// Noise�p�J�E���g�_�E���l
	int NoiseCycle;	// Noise���� 32*2^25(0) �` 1*2^25(31) NoiseCycle==0�̎��̓m�C�Y�I�t
	int NoiseValue;	// �m�C�Y�l  1 or -1

	// �G���x���[�v�֌W
	int	Xr_stat;
	int	Xr_el;
	int	Xr_step;
	int	Xr_and;
	int	Xr_cmp;
	int	Xr_add;
	int Xr_limit;
	
	
	int	Note;	// ���K (0 <= Note < 10*12)
	int	Kc;		// ���K (1 <= Kc <= 128)
	int	Kf;		// ������ (0 <= Kf < 64)
	int Ar;		// 0 <= Ar < 31
	int D1r;	// 0 <= D1r < 31
	int	D2r;	// 0 <= D2r < 31
	int	Rr;		// 0 <= Rr < 15
	int	Ks;		// 0 <= Ks <= 3
	int	Dt2;	// Pitch �ɑ΂���␳��(0, 384, 500, 608)
	int	Dt1;	// DT1�̒l(0�`7)
	int Nfrq;	// Noiseflag,NFRQ�̒l

	struct {int val_and,cmp,add, limit;}
		StatTbl[RELEASE_MAX+1];	// ��Ԑ��ڃe�[�u��
	//           ATACK     DECAY   SUSTAIN     SUSTAIN_MAX RELEASE     RELEASE_MAX
	// and     :                               4097                    4097
	// cmp     :                               2048                    2048
	// add     :                               0                       0
	// limit   : 0         D1l     63          63          63          63
	// nextstat: DECAY     SUSTAIN SUSTAIN_MAX SUSTAIN_MAX RELEASE_MAX RELEASE_MAX

	 void	CulcArStep();
	 void	CulcD1rStep();
	 void	CulcD2rStep();
	 void	CulcRrStep();
	 void	CulcPitch();
	 void	CulcDt1Pitch();
	 void CulcNoiseCycle();

public:
	Op(void);
	~Op() {};
	void Init();
	 void InitSamprate();
	 void SetFL(int n);
	 void SetKC(int n);
	 void SetKF(int n);
	 void SetDT1MUL(int n);
	 void SetTL(int n);
	 void SetKSAR(int n);
	 void SetAMED1R(int n);
	 void SetDT2D2R(int n);
	 void SetD1LRR(int n);
	 void KeyON();
	 void KeyOFF();
	 void Envelope(int env_counter);
	 void SetNFRQ(int nfrq);

	 void Output0(int lfopitch, int lfolevel);		// �I�y���[�^0�p
	 void Output(int lfopitch, int lfolevel);		// ��ʃI�y���[�^�p
	 void Output32(int lfopitch, int lfolevel);		// �X���b�g32�p
	 void Output0_22(int lfopitch, int lfolevel);		// �I�y���[�^0�p
	 void Output_22(int lfopitch, int lfolevel);		// ��ʃI�y���[�^�p
	 void Output32_22(int lfopitch, int lfolevel);		// �X���b�g32�p
};
#define __CLASSOP__
#endif
