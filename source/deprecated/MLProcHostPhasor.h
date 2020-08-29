
// MadronaLib: a C++ framework for DSP applications.
// Copyright (c) 2013 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

#ifndef ML_PROC_HOST_PHASOR_H
#define ML_PROC_HOST_PHASOR_H

#include "MLProc.h"

// ----------------------------------------------------------------
// class definition

class MLProcHostPhasor : public MLProc
{
public:
	 MLProcHostPhasor();
	~MLProcHostPhasor();

	// Set the time and bpm. The time refers to the start of the current engine processing block.
	void setTimeAndRate(const double secs, const double position, const double bpm, bool isPlaying);
	
	void clear() override;
	void process() override;		
	MLProcInfoBase& procInfo() override { return mInfo; }

private:
	MLProcInfo<MLProcHostPhasor> mInfo;
	void doParams(void);
  MLSample mOmega{0};

  
  bool _playing1{false};
  bool _active1 {false};
	//float mSr;
	
	double mDpDt;
	double mPhase1;
	size_t _samplesSincePreviousTime;
  double _ppqPos1;
  double _ppqPhase1;
};

#endif // ML_PROC_HOST_PHASOR_H
