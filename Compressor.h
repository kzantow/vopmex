#pragma once

class Compressor
{
public:
	Compressor();
	~Compressor();
	
	void SetSamplerate(float newrate);
	void Reset(void);
	void setThresh(float value);
	void setRatio(float value);
	void setAR(float value);
	void setRR(float value);
	void setSoftKnee(bool value) { softKnee = value; }
	float GetVolEnv(float input);
	float CalcReduction(float input);
private:
	float volume_curb_soft(float env);
	float volume_curb_hard(float env);
	
  private:
	float	samplerate;
	
	float	envelope;
	float	coeffRR, coeffRR2, coeffAR, coeffAR2;
	float	rt, at;
	
	float	inputGain, outputGain, Ratio, Ratio2, Thresh;
	bool	softKnee;
};
