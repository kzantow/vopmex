//m_puusan���쐬��X68sound.dll�̃\�[�X�����ς��Ă܂��B
//���ώҁ@sam ���ϓ� 2002.3.8
//���ϗv�_
//1)borand C++builder �ŃR���p�C�����邽��__asm�����폜
//2)AD-PCM�����폜
//3)Opm�^�C�}�[�֘A���폜
//4)Win�^�C�}�[�A�X���b�h�֘A�̑��x�������폜
//5)Opm DMA�֘A���폜
//5)VST�ɑΉ������邽�߁AOpm�R�}���h�o�b�t�@�̍\���ɃT���v���|�C���g��ǉ��B
//6)VST�ɑΉ������邽�߁A�T���v�����O���[�g��20.5K,44.1K�ȊO�ɂ��Ή��\�ɂ���B
//7)VST�ɑΉ������邽�߁Awin�ˑ������폜

#include	<stdio.h>
//#include	<stdlib.h>
#include	<math.h>
//#include	<windows.h>
//#include	<windowsx.h>
//#include	<conio.h>
//#include	<ctype.h>

#define	N_CH	8

#define	PRECISION_BITS	(10)
#define	PRECISION	(1<<PRECISION_BITS)
#define	SIZEALPHATBL_BITS	(10)
#define	SIZEALPHATBL	(1<<SIZEALPHATBL_BITS)

#define	SIZESINTBL_BITS	(10)
#define	SIZESINTBL	(1<<SIZESINTBL_BITS)
#define	MAXSINVAL	(1<<(SIZESINTBL_BITS+2))

#define	PI	3.1415926535897932
#define	MAXELVAL_BITS	(30)
#define	MAXELVAL	(1<<MAXELVAL_BITS)
#define	MAXARTIME_BITS	(20)
#define	SIZEARTBL	(100)
#define	MAXARTIME	(SIZEARTBL*(1<<MAXARTIME_BITS))

extern int	STEPTBL[11*12*64];
extern int	STEPTBL3[11*12*64];
#define	ALPHAZERO	(SIZEALPHATBL*3)

extern unsigned short	ALPHATBL[ALPHAZERO+SIZEALPHATBL+1];
extern short	SINTBL[SIZESINTBL];

extern int STEPTBL_O2[12*64];
extern int	D1LTBL[16];
extern int	DT1TBL[128+4];
extern int	DT1TBL_org[128+4];

typedef struct {
int	val_and;
int	add;
}	XR_ELE;

extern XR_ELE XRTBL[64+32];

extern int DT2TBL[4];
extern unsigned short	NOISEALPHATBL[ALPHAZERO+SIZEALPHATBL+1];

