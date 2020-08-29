
// MadronaLib: a C++ framework for DSP applications.
// Copyright (c) 2013 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

#include "MLProcHostPhasor.h"

// ----------------------------------------------------------------
// registry section

namespace
{
	MLProcRegistryEntry<MLProcHostPhasor> classReg("host_phasor");
	ML_UNUSED MLProcOutput<MLProcHostPhasor> outputs[] = { "out" };
}			

// ----------------------------------------------------------------
// implementation

MLProcHostPhasor::MLProcHostPhasor() :
mDpDt(0),
mPhase1(0.),
_samplesSincePreviousTime(0)
{
	clear();
//	//debug() << "MLProcHostPhasor constructor\n";
}

MLProcHostPhasor::~MLProcHostPhasor()
{
//	//debug() << "MLProcHostPhasor destructor\n";
}

void MLProcHostPhasor::doParams(void) 
{
	mParamsChanged = false;
}

// set input parameters from host info
void MLProcHostPhasor::setTimeAndRate(const double secs, const double ppqPos, const double bpm, bool isPlaying)
{
	// working around a bug I can't reproduce, so I'm covering all the bases.
	if ( ((ml::isNaN(ppqPos)) || (ml::isInfinite(ppqPos)))
		|| ((ml::isNaN(bpm)) || (ml::isInfinite(bpm)))
		|| ((ml::isNaN(secs)) || (ml::isInfinite(secs))) ) 
	{
    debug() << "MLProcHostPhasor::setTimeAndRate: bad input! \n";
		return;
	}
  
  bool active = (_ppqPos1 != ppqPos) && isPlaying;
  bool justStarted = isPlaying && !_playing1;

  double ppqPhase = 0.;
  
	if (active)
	{
    if(ppqPos > 0.f)
    {
      ppqPhase = ppqPos - floor(ppqPos);
    }
    else
    {
      ppqPhase = ppqPos;
    }
    
    mOmega = ppqPhase;

    if(justStarted)
    {
      mDpDt = 0.;
    }
    else
    {
      double dPhase = ppqPhase - _ppqPhase1;
      if(dPhase < 0.)
      {
        dPhase += 1.;
      }
      mDpDt = ml::clamp(dPhase/static_cast<double>(_samplesSincePreviousTime), 0., 1.);
    }
	}
	else
	{
    mOmega = -1.f;
		mDpDt = 0.;
	}
  
  _ppqPos1 = ppqPos;
  _ppqPhase1 = ppqPhase;
  _active1 = active;
  _playing1 = isPlaying;
	_samplesSincePreviousTime = 0;
}

void MLProcHostPhasor::clear()
{
  mDpDt = 0.;
	_active1 = false;
	_playing1 = 0;
}

// generate a quarter-note phasor from the input parameters
void MLProcHostPhasor::process()
{
	MLSignal& y = getOutput();
	for (int n=0; n<kFloatsPerDSPVector; ++n)
	{
    y[n] = mOmega;
    mOmega += mDpDt;
		if(mOmega > 1.f) 
		{
			mOmega -= 1.f;
		}
	}
	_samplesSincePreviousTime += kFloatsPerDSPVector;
}
  
