#include <math.h>
#include "Compressor.h"

#define undenormalise(sample) if(((*(unsigned int*)&sample)&0x7f800000)==0) sample=0.0f
float fastExp(float x);

Compressor::Compressor()
: samplerate(44100)
, envelope(.0f)
, softKnee(false)
{
}

Compressor::~Compressor()
{
}

void Compressor::SetSamplerate(float newrate)
{
	samplerate = newrate;
	
	coeffAR = expf(-1.0f/(at*samplerate));
	coeffAR2 = 1.0f - coeffAR;
	coeffRR = expf(-1.0f/(rt*samplerate));
	coeffRR2 = 1.0f - coeffRR;
}

//“ü—Í’l‚Í‚O`‚P
void Compressor::setThresh(float value)
{
	Thresh = value;
}


//“ü—Í’l‚Í‚O`‚P
void Compressor::setRatio(float value)
{
	Ratio = 1.0f - value;
	Ratio2 = value;
}

//“ü—Í’l‚Í•b’PˆÊ
void Compressor::setAR(float value)
{
	if (at != value) {
		at = value;
		coeffAR = expf(-1.0f/(at*samplerate));
		coeffAR2 = 1.0f  - coeffAR;
	}
}

//“ü—Í’l‚Í•b’PˆÊ
void Compressor::setRR(float value)
{
	if (rt != value) {
		rt = value;
		coeffRR = expf(-1.0f/(rt*samplerate));
		coeffRR2 = 1.0f - coeffRR;
	}
}

void Compressor::Reset(void)
{
	envelope = 0.0f;
}

float Compressor::GetVolEnv(float input)
{
	float	env=.0f;
	int		abstemp;
	
	abstemp = (*(int*)&input)&0x7fffffff;	//‚‘¬abs
	env = *(float*)&abstemp;

	if (env < envelope) {
		envelope *= coeffRR;
		envelope += coeffRR2*env;
		
	}
	else {
		envelope *= coeffAR;
		envelope += coeffAR2*env;
	}
	undenormalise(envelope);

	env = envelope;
	
	return env;
}

float Compressor::CalcReduction(float input)
{
	float env = GetVolEnv(input);
	float out = softKnee?volume_curb_soft(env):volume_curb_hard(env);
	return out;
}

inline float Compressor::volume_curb_soft(float env)
{
	if (env == 0.0f) {
		return (1.0f);
	}
	else {
		return (Ratio + (Ratio2*(1.0f-1.0f/fastExp(env/Thresh))*Thresh)/env);
	}
}

inline float Compressor::volume_curb_hard(float env)
{
	if (env > Thresh) {
		return ((Thresh*Ratio2/env)+Ratio);
	}
	else {
		return (1.0f);
	}
}

float fastExp(float x)
{
	float fResult = 0.715f;
	fResult *= x;
	fResult += 1.0f;
	fResult *= x;
	fResult += 1.0f;
	return fResult;
}
