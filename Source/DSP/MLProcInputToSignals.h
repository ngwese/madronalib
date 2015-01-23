
// MadronaLib: a C++ framework for DSP applications.
// Copyright (c) 2013 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

#ifndef ML_PROC_MIDI_TO_SIGNALS_H
#define ML_PROC_MIDI_TO_SIGNALS_H

#include "AppConfig.h"

#include "MLDSP.h"
#include "MLProc.h"
#include "MLScale.h"
#include "MLChangeList.h"
#include "MLInputProtocols.h"
#include "MLControlEvent.h"
#include "MLT3DHub.h"
#include "MLDefaultFileLocations.h"

#include "pa_ringbuffer.h"

#include <stdexcept>

// a voice that can play.
//
class MLVoice
{
public:
	
	enum State
	{
		kOff,
		kOn,
		kSustain
	};
	
	MLVoice();
	~MLVoice() {} ;
	void clearState();
	void clearChanges();
	MLProc::err resize(int size);
	void zero();

	// send a note on, off or sustain event to the voice.
	void addNoteEvent(const MLControlEvent& e, const MLScale& scale);
	void stealNoteEvent(const MLControlEvent& e, const MLScale& scale, bool retrig);

	int mState;
    int mInstigatorID; // for matching event sources, could be MIDI key, or touch number.
    int mChannel;   
	int mNote;
	int mAge;	// time active, measured to the end of the current process buffer
	
	// for continuous touch inputs (OSC)
	float mStartX;
	float mStartY;
	float mStartVel;
	float mPitch;
	float mX1;
	float mY1;
	float mZ1;
    
	MLChangeList mdPitch;
	MLChangeList mdPitchBend;
	MLChangeList mdGate;
	MLChangeList mdAmp;
	MLChangeList mdVel;
	MLChangeList mdNotePressure;
	MLChangeList mdChannelPressure;
	MLChangeList mdMod;
	MLChangeList mdMod2;
	MLChangeList mdMod3;
	MLChangeList mdDrift;
	
	MLControlEvent mCurrentNoteEvent;
};

extern const int kNumVoiceSignals;
extern const char * voiceSignalNames[];

class MLProcInputToSignals : public MLProc
{
public:

	static const float kControllerScale;
	static const float kDriftConstantsAmount;
	static const float kDriftRandomAmount;

	static const int kFrameBufferSize = 128;

    MLProcInputToSignals();
	~MLProcInputToSignals();
	MLProcInfoBase& procInfo() { return mInfo; }
	int getOutputIndex(const MLSymbol name);

	void setInputFrameBuffer(PaUtilRingBuffer* pBuf);
	void clear();
	MLProc::err prepareToProcess();
	void process(const int n);
    
	void clearChangeLists();
    void setEventTimeOffset(int t);
    void setEventRange(MLControlEventVector::const_iterator start, MLControlEventVector::const_iterator end);
	
 	void setup();
 	err resize();
	
	void doParams();

private:
    void processOSC(const int n);
	void processEvents();
	void writeOutputSignals(const int n);

    void processEvent(const MLControlEvent& event);
	void doNoteOn(const MLControlEvent& event);
	void doNoteOff(const MLControlEvent& event);
	void doSustain(const MLControlEvent& event);
	void doController(const MLControlEvent& event);
	void doPitchWheel(const MLControlEvent& event);
	void doNotePressure(const MLControlEvent& event);
	void doChannelPressure(const MLControlEvent& event);

	void dumpEvents();
	void dumpVoices();
	void dumpSignals();
    
    int findFreeVoice();
    int findOldestSustainedVoice();
    int findOldestVoice();

	int mProtocol;
	MLProcInfo<MLProcInputToSignals> mInfo;
	PaUtilRingBuffer* mpFrameBuf;
	MLSignal mLatestFrame;
    
    MLControlEventVector mNoteEventsPlaying;    // notes with keys held down and sounding
    MLControlEventStack mNoteEventsPending;    // notes stolen that may play again when voices are freed
    
	MLVoice mVoices[kMLEngineMaxVoices];

	int mNextEventIdx;
	int mVoiceRotateOffset;
	
    int mEventTimeOffset;
    // the range of events [mStartEvent, mEndEvent) will control the next process() call.
    MLControlEventVector::const_iterator mStartEvent, mEndEvent;
		
	int mControllerNumber;
	int mCurrentVoices;
	int mDriftCounter;
	int mEventCounter;
    int mFrameCounter;
		
	MLRange mPitchRange;
	MLRange mAmpRange;
	bool mRetrig;
	bool mUnisonMode;
	bool mRotateMode;
	bool mAddChannelPressure;
	int mUnisonInputTouch;
	float mUnisonVel;
	float mGlide;
	int mOSCDataRate;
	
	float mUnisonPitch1;

	MLSignal mTempSignal;
	MLSignal mChannelAfterTouchSignal;

	float mPitchWheelSemitones;
	MLScale mScale;
	
	int temp;
	bool mSustainPedal;
	std::string mScalePath;
};


#endif // ML_PROC_MIDI_TO_SIGNALS_H