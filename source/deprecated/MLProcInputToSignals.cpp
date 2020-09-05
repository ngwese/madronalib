// MadronaLib: a C++ framework for DSP applications.
// Copyright (c) 2013 Madrona Labs LLC. http://www.madronalabs.com
// Distributed under the MIT license: http://madrona-labs.mit-license.org/

#include "MLProcInputToSignals.h"

const ml::Symbol voicesSym("voices");
const ml::Symbol data_rateSym("data_rate");
const ml::Symbol scaleSym("scale");
const ml::Symbol protocolSym("protocol");
const ml::Symbol bendSym("bend");
const ml::Symbol modSym("mod");
const ml::Symbol modMPEXSym("mod_mpe_x");
const ml::Symbol unisonSym("unison");
const ml::Symbol glideSym("glide");

const int kMaxEvents = 1 << 4;//1 << 8; // max events per signal vector
static const int kNumVoiceSignals = 8;
const ml::Symbol voiceSignalNames[kNumVoiceSignals]
{
  "pitch",
  "gate",
  "vel",
  "voice",
  "after",
  "moda",
  "modb",
  "modc"
};

const float MLProcInputToSignals::kControllerScale = 1.f/127.f;
#if INPUT_DRIFT
const float kDriftConstants[16] =
{
  0.465f, 0.005f, 0.013f, 0.019f,
  0.155f, 0.933f, 0.002f, 0.024f,
  0.943f, 0.924f, 0.139f, 0.501f,
  0.196f, 0.591f, 0.961f, 0.442f
};
const float MLProcInputToSignals::kDriftConstantsAmount = 0.004f;
const float MLProcInputToSignals::kDriftRandomAmount = 0.002f;
#endif

// ----------------------------------------------------------------
//
#pragma mark MLVoice
//

static const int kDriftInterval = 10;

MLVoice::MLVoice()
{
  clearState();
  clearChanges();
}

void MLVoice::setSampleRate(float sr)
{
  mdDrift.setSampleRate(sr);
  mdPitch.setSampleRate(sr);
  mdPitchBend.setSampleRate(sr);
  mdGate.setSampleRate(sr);
  mdAmp.setSampleRate(sr);
  mdVel.setSampleRate(sr);
  mdNotePressure.setSampleRate(sr);
  mdChannelPressure.setSampleRate(sr);
  mdMod.setSampleRate(sr);
  mdMod2.setSampleRate(sr);
  mdMod3.setSampleRate(sr);
}

void MLVoice::resize(int bufSize)
{
  // make delta lists
  // allow for one change each sample, though this is unlikely to get used.
  mdPitch.setDims(bufSize);
  mdPitchBend.setDims(bufSize);
  mdGate.setDims(bufSize);
  mdAmp.setDims(bufSize);
  mdVel.setDims(bufSize);
  mdNotePressure.setDims(bufSize);
  mdChannelPressure.setDims(bufSize);
  mdMod.setDims(bufSize);
  mdMod2.setDims(bufSize);
  mdMod3.setDims(bufSize);
  mdDrift.setDims(bufSize);
}

void MLVoice::clearState()
{
  mState = kOff;
  mInstigatorID = 0;
  mNote = 0.;
  mAge = 0;
  mChannel = 0;
  mStartX = 0.;
  mStartY = 0.;
  mStartVel = 0.;
  mPitch = 0.;
  mX1 = 0.;
  mY1 = 0.;
  mZ1 = 0.;
}

// clear changes but not current state.
void MLVoice::clearChanges()
{
  mdDrift.clearChanges();
  mdPitch.clearChanges();
  mdPitchBend.clearChanges();
  mdGate.clearChanges();
  mdAmp.clearChanges();
  mdVel.clearChanges();
  mdNotePressure.clearChanges();
  mdChannelPressure.clearChanges();
  mdMod.clearChanges();
  mdMod2.clearChanges();
  mdMod3.clearChanges();
}

void MLVoice::zero()
{
  mdDrift.zero();
  mdPitch.zero();
  mdPitchBend.zero();
  mdGate.zero();
  mdAmp.zero();
  mdVel.zero();
  mdNotePressure.zero();
  mdChannelPressure.zero();
  mdMod.zero();
  mdMod2.zero();
  mdMod3.zero();
}

void MLVoice::zeroExceptPitch()
{
  mdDrift.zero();
  mdGate.zero();
  mdAmp.zero();
  mdVel.zero();
  mdNotePressure.zero();
  mdChannelPressure.zero();
  mdMod.zero();
  mdMod2.zero();
  mdMod3.zero();
}

void MLVoice::zeroPressure()
{
  mdGate.zero();
  mdAmp.zero();
  mdVel.zero();
  mdNotePressure.zero();
  mdChannelPressure.zero();
}



void MLVoice::addNoteEvent(const MLControlEvent& e, const MLScale& scale)
{
  auto time = e.mTime;

  switch(e.mType)
  {
    case MLControlEvent::kNoteOn:
      // start new note, setting note and velocity from OSC or MIDI
      mState = kOn;
      mNote = e.mValue1;

     // std::cout << "add note event ON : inst: " << e.mID << " pitch " << mNote << " vel " << e.mValue2 << "\n";
      mdPitch.addChange(scale.noteToLogPitch(mNote), time);
      mdGate.addChange(1.f, time);
      mdVel.addChange(e.mValue2, time);
      
      // set start x and y from OSC
      mStartX = e.mValue3;
      mStartY = e.mValue4;
      mAge = 0;
      mInstigatorID = e.mID;
      mChannel = e.mChannel;
      break;
      
    case MLControlEvent::kNoteUpdate:
      // update note, z, x, y from OSC

      // update note and z
      mdPitch.addChange(scale.noteToLogPitch(e.mValue1), time);
      mdAmp.addChange(e.mValue2, time);
      
      // KEY outputs: dy, x, y
      // TODO add UI / settings for relative or absolute options
      mdMod.addChange(e.mValue4 - mStartY, time);
      mdMod2.addChange(e.mValue3*2.f - 1.f, time);
      mdMod3.addChange(e.mValue4*2.f - 1.f, time);
      break;
      
    case MLControlEvent::kNoteSustain:
      // sent when note is released iwth sustain pedal on
      // no signal changes, but changes state to sustain
      mState = kSustain;
      break;
    case MLControlEvent::kNoteOff:
    default:
      mState = kOff;
      
    //  std::cout << "add note event OFF: inst: " << e.mID << " pitch " << mNote << "\n";
      mdGate.addChange(0.f, time);
      mdAmp.addChange(0.f, time);
      mdVel.addChange(0.f, time);
      mdNotePressure.addChange(0.f, time);
      
      // for MPE mode when controlling envelopes with aftertouch: ensure
      // notes are not sending pressure when off
      mdChannelPressure.addChange(0.f, time);
      mAge = 0;
      
      // we leave mChannel alone so that pitch bends will retain their values when the note ends
      
      break;
  }
  
  
  
  mCurrentUnisonNoteEvent = e;
}


void MLVoice::stealNoteEvent(const MLControlEvent& e, const MLScale& scale, bool retrig)
{
  float note = e.mValue1;
  float vel = e.mValue2;
  auto time = e.mTime;
  if (time == 0) time++; // in case where time = 0, make room for retrigger.
  
  mState = kOn;
  mInstigatorID = e.mID;
  mNote = note;
  mAge = 0;
  mdPitch.addChange(scale.noteToLogPitch(note), time);
  
  if (retrig)
  {
    mdGate.addChange(0.f, time - 1);
    mdNotePressure.addChange(0.f, time - 1);
    //mdChannelPressure.addChange(0.f, time - 1);
    //mdVel.addChange(0.f, time - 1);
  }
  
  mdGate.addChange(1, time);
  mdAmp.addChange(vel, time);
  mdVel.addChange(vel, time);
  
  mCurrentUnisonNoteEvent = e;

}



#pragma mark -
// ----------------------------------------------------------------
// registry section

namespace
{
  MLProcRegistryEntry<MLProcInputToSignals> classReg("midi_to_signals");
  ML_UNUSED MLProcParam<MLProcInputToSignals> params[11] = { "bufsize", "voices", "bend", "mod", "mod_mpe_x", "unison", "glide", "protocol", "data_rate" , "scale", "master_tune"};
  // no input signals.
  ML_UNUSED MLProcOutput<MLProcInputToSignals> outputs[] = {"*"};  // variable outputs
}

MLProcInputToSignals::MLProcInputToSignals() :
mProtocol(-1),
mControllerNumber(-1),
mControllerMPEXNumber(73),
mCurrentVoices(0),
mFrameCounter(0),
mGlissando(false),
mUnisonInputTouch(-1),
mUnisonVel(0.),
//mFirstEvent(nullptr),
//mLastEvent(nullptr),
mSustainPedal(false)
{
  setParam("voices", 0);  // default
  setParam("protocol", kInputProtocolMIDI);  // default
  setParam("data_rate", 100);  // default

  mVoiceRotateOffset = 0;
  mNextEventIdx = 0;
  mEventTimeOffset = 0;

  mPitchWheelSemitones = 7.f;
  mUnisonMode = false;
  mRotateMode = true;
  mEventCounter = 0;
  mDriftCounter = -1;

  temp = 0;
  mOSCDataRate = 100;

  // TODO lockfree queue template
  //  mEventData.resize(kMaxEvents);
  //  PaUtil_InitializeRingBuffer( &mEventQueue, sizeof(MLControlEvent), kMaxEvents, &(mEventData[0]) );

  /*
   int remaining = PaUtil_GetRingBufferReadAvailable(&mFileActionQueue);

   FileAction a;
   int filesRead = 0;


   if((filesRead = PaUtil_ReadRingBuffer( &mFileActionQueue, &a, 1 )))
   {
   if(a.mFile.exists())
   {
   doFileQueueAction(a);
   }
   int filesInQueue = PaUtil_GetRingBufferReadAvailable(&mFileActionQueue);
   mMaxFileQueueSize = ml::max(filesInQueue, mMaxFileQueueSize);
   if(!filesInQueue) endConvertPresets();
   }


   // add file action to queue
   FileAction f(action, fileToProcess, &collection, idx, size);
   PaUtil_WriteRingBuffer( &mFileActionQueue, &f, 1 );


   */

  mNoteEventsPlaying.resize(kMaxEvents);
  mNoteEventsPending.resize(kMaxEvents);
}

MLProcInputToSignals::~MLProcInputToSignals()
{
}

// set frame buffer for OSC inputs
void MLProcInputToSignals::setInputFrameBuffer(Queue<TouchFrame>* pBuf)
{
  mpFrameBuf = pBuf;
}

// needs to be executed by every process() call to clear changes from change lists.
void MLProcInputToSignals::clearChangeLists()
{
  // things per voice
  int maxVoices = getContext()->getRootContext()->getMaxVoices();
  for (int v=0; v<maxVoices; ++v)
  {
    mVoices[v].clearChanges();
  }
  mMPEMainVoice.clearChanges();
  
  for(auto& c : mPitchBendChangesByChannel)
  {
    c.clearChanges();
  }

}

// set up output buffers
MLProc::err MLProcInputToSignals::resize()
{
  float sr = getContextSampleRate();
  static const ml::Symbol bufsizeSym("bufsize");

  MLProc::err re = OK;

  // resize voices
  //
  int bufSize = (int)getParam(bufsizeSym);
  int vecSize = getContextVectorSize();

  int maxVoices = getContext()->getRootContext()->getMaxVoices();
  mVoices.resize(maxVoices);

  MLProc::err r;
  for(int i=0; i<maxVoices; ++i)
  {
    mVoices[i].setSampleRate(sr);
    mVoices[i].resize(bufSize);
  }
  mMPEMainVoice.resize(bufSize);
  
  // make signals that apply to all voices
  mTempSignal.setDims(vecSize);
  mMainPitchSignal.setDims(vecSize);
  mMainChannelPressureSignal.setDims(vecSize);
  mMainModSignal.setDims(vecSize);
  mMainMod2Signal.setDims(vecSize);
  mMainMod3Signal.setDims(vecSize);

  // make outputs
  //
  for(int i=1; i <= maxVoices * kNumVoiceSignals; ++i)
  {
    if (!outputIsValid(i))
    {
      setOutput(i, getContext()->getNullOutput());
    }
  }

  // do voice params
  //
  for(int i=0; i<maxVoices; ++i)
  {
    if((i*kNumVoiceSignals + 1) < getNumOutputs())
    {
      // set initial pitch to 0.
      mVoices[i].mdPitch.addChange(0.f, 0);
      MLSignal& out = getOutput(i*kNumVoiceSignals + 1);
      mVoices[i].mdPitch.writeToSignal(out, vecSize);
      mVoices[i].mdPitchBend.addChange(0.f, 0);
      mVoices[i].mdDrift.setGlideTime(kDriftInterval);
    }
  }

  for(int i = 0; i < kMPEInputChannels; ++i)
  {
    mPitchBendChangesByChannel[i].setSampleRate(sr);
    mPitchBendChangesByChannel[i].setDims(vecSize);
    mPitchBendSignals[i].setDims(vecSize);
  }

  clearChangeLists();
  return re;
}

// it's uncommon for a processor to override MLProc::getOutputIndex.
// But unlike overriding MLProc::getOutput, it's possible.
// we do it here because we have a variable number of outputs and would
// like to make names for them procedurally.
int MLProcInputToSignals::getOutputIndex(const ml::Symbol name)
{
  // voice numbers are 1-indexed.
  int idx = 0;
  int voice = 0;
  int sig = 0;

  ml::Symbol name0 = ml::textUtils::stripFinalNumber(name);

  // match signal name with symbol text
  for(int n=0; n<kNumVoiceSignals; ++n)
  {
    if(name0 == voiceSignalNames[n])
    {
      sig = n + 1;
      break;
    }
  }

  // get voice number from end of symbol
  if (sig)
  {
    voice = ml::textUtils::getFinalNumber(name);
    if ((voice) && (voice <= mCurrentVoices))
    {
      idx = (voice - 1)*kNumVoiceSignals + sig;
    }
  }

  if (!idx)
  {
    //debug() << "MLProcInputToSignals::getOutputIndex: null output " << name << "\n";
  }

  return idx;
}

void MLProcInputToSignals::setup()
{
  doParams();
}

void MLProcInputToSignals::doParams()
{
  int maxVoices = getContext()->getRootContext()->getMaxVoices();
  int newVoices = (int)getParam(voicesSym);
  newVoices = ml::clamp(newVoices, 0, 15);

  // TODO enable / disable voice containers here
  mOSCDataRate = (int)getParam(data_rateSym);

  const ml::Text& scaleName = getTextParam(scaleSym);
  mScale.loadFromRelativePath(scaleName);

  mMasterTune = getParam("master_tune");
  if(within(mMasterTune, 220.f, 880.f))
  {
    mMasterPitchOffset = log2f(mMasterTune/440.f);
  }

  const int newProtocol = (int)getParam(protocolSym);
  mProtocol = newProtocol;

  mGlide = getParam(glideSym);
  for (int v=0; v<maxVoices; ++v)
  {
    mVoices[v].mdPitch.setGlideTime(mGlide);
    mVoices[v].mdPitchBend.setGlideTime(mGlide);
  }
  mMPEMainVoice.mdPitchBend.setGlideTime(mGlide);

  float oscGlide = (1.f / std::max(100.f, (float)mOSCDataRate));

  switch(mProtocol)
  {
    case kInputProtocolOSC:
      for(int i=0; i<maxVoices; ++i)
      {
        mVoices[i].mdGate.setGlideTime(0.0f);
        mVoices[i].mdAmp.setGlideTime(oscGlide);
        mVoices[i].mdVel.setGlideTime(0.0f);
        mVoices[i].mdNotePressure.setGlideTime(oscGlide);
        mVoices[i].mdChannelPressure.setGlideTime(oscGlide);
        mVoices[i].mdMod.setGlideTime(oscGlide);
        mVoices[i].mdMod2.setGlideTime(oscGlide);
        mVoices[i].mdMod3.setGlideTime(oscGlide);
      }
      break;
    case kInputProtocolMIDI:
    case kInputProtocolMIDI_MPE:
      for(int i=0; i<maxVoices; ++i)
      {
        mVoices[i].mdGate.setGlideTime(0.f);
        mVoices[i].mdAmp.setGlideTime(0.001f);
        mVoices[i].mdVel.setGlideTime(0.f);
        mVoices[i].mdNotePressure.setGlideTime(0.001f);
        mVoices[i].mdChannelPressure.setGlideTime(0.001f);
        mVoices[i].mdMod.setGlideTime(0.001f);
        mVoices[i].mdMod2.setGlideTime(0.001f);
        mVoices[i].mdMod3.setGlideTime(0.001f);
      }
      break;
  }

  if (newVoices != mCurrentVoices)
  {
    mCurrentVoices = newVoices;
    clear();
  }

  // pitch wheel mult
  mPitchWheelSemitones = getParam(bendSym);

  // listen to controller number mods
  mControllerNumber = (int)getParam(modSym);
  mControllerMPEXNumber = (int)getParam(modMPEXSym);
  
  int unison = (int)getParam(unisonSym);
  if (mUnisonMode != unison)
  {
    mUnisonMode = unison;
    clear();
  }

  mParamsChanged = false;
  //dumpParams();  // DEBUG
}

MLProc::err MLProcInputToSignals::prepareToProcess()
{
  clear();
  return OK;
}

void MLProcInputToSignals::clear()
{
  int vecSize = getContextVectorSize();
  int maxVoices = getContext()->getRootContext()->getMaxVoices();

  // resize if needed, a hack
  if(mVoices.size() != maxVoices)
  {
    resize();
  }
  clearChangeLists();

  for(int i=0; i<kMaxEvents; ++i)
  {
    mNoteEventsPlaying[i].clear();
    mNoteEventsPending[i].clear();
  }

  int outs = getNumOutputs();
  if (outs)
  {
    for (int v=0; v<maxVoices; ++v)
    {
      mVoices[v].clearState();
      mVoices[v].clearChanges();
//      mVoices[v].zeroExceptPitch();
      mVoices[v].zeroPressure();

      MLSignal& pitch = getOutput(v*kNumVoiceSignals + 1);
      MLSignal& gate = getOutput(v*kNumVoiceSignals + 2);
      MLSignal& velSig = getOutput(v*kNumVoiceSignals + 3);
      MLSignal& voiceSig = getOutput(v*kNumVoiceSignals + 4);
      MLSignal& after = getOutput(v*kNumVoiceSignals + 5);
      MLSignal& mod = getOutput(v*kNumVoiceSignals + 6);
      MLSignal& mod2 = getOutput(v*kNumVoiceSignals + 7);
      MLSignal& mod3 = getOutput(v*kNumVoiceSignals + 8);

      mVoices[v].mdPitch.writeToSignal(pitch, vecSize);
      mVoices[v].mdGate.writeToSignal(gate, vecSize);
      mVoices[v].mdVel.writeToSignal(velSig, vecSize);
      voiceSig.setToConstant(v);

      mVoices[v].mdNotePressure.writeToSignal(after, vecSize);
      mVoices[v].mdChannelPressure.writeToSignal(after, vecSize);
      mVoices[v].mdAmp.writeToSignal(after, vecSize);

      mVoices[v].mdMod.writeToSignal(mod, vecSize);
      mVoices[v].mdMod2.writeToSignal(mod2, vecSize);
      mVoices[v].mdMod3.writeToSignal(mod3, vecSize);
    }
    mMPEMainVoice.clearState();
    mMPEMainVoice.clearChanges();
//    mMPEMainVoice.zeroExceptPitch();
    mMPEMainVoice.zeroPressure();
  }
  mEventCounter = 0;
}


// order of signals:
// pitch
// gate
// amp (gate * velocity)
// vel (velocity, stays same after note off)
// voice
// aftertouch
// mod, mod2, mod3

// display MIDI: pitch gate vel voice after mod -2 -3 -4
// display OSC: pitch gate vel(constant during hold) voice(touch) after(z) dx dy x y

void MLProcInputToSignals::process()
{
  if (mParamsChanged) doParams();
  int sr = getContextSampleRate();
  clearChangeLists();

#if INPUT_DRIFT
  // update drift change list for each voice
  if ((mDriftCounter < 0) || (mDriftCounter > sr*kDriftInterval))
  {
    for (int v=0; v<mCurrentVoices; ++v)
    {
      float drift = (kDriftConstants[v] * kDriftConstantsAmount) + (mRand.getSample()*kDriftRandomAmount);
      mVoices[v].mdDrift.addChange(drift, 1);
    }
    mDriftCounter = 0;
  }
  mDriftCounter += kFloatsPerDSPVector;
#endif

  // update age for each voice
  for (int v=0; v<mCurrentVoices; ++v)
  {
    if(mVoices[v].mAge >= 0)
    {
      mVoices[v].mAge += kFloatsPerDSPVector;
    }
  }

  // generate change lists from events

  
  processEvents();

  /*
  switch(mProtocol)
  {
    case kInputProtocolOSC:
      processOSC(kFloatsPerDSPVector);
      break;
    case kInputProtocolMIDI:
    case kInputProtocolMIDI_MPE:
      processEvents();
      break;
  }
*/
  
  // generate output signals from change lists
  writeOutputSignals(kFloatsPerDSPVector);

  mFrameCounter += kFloatsPerDSPVector;
  if(mFrameCounter > sr)
  {
    //dumpEvents();
    //dumpVoices();
    //dumpSignals();
    //dumpTouchFrame();
    mFrameCounter -= sr;
  }
}


// moving this to the implementation from header fixed an issue where time was not getting updated.
// mystery bug.
void MLProcInputToSignals::setVectorStartTime(uint64_t t)
{
  mVectorStartTime = t;
//  std::cout << "vst: " << t << "\n";
}

// process control events to make change lists
//
void MLProcInputToSignals::processEvents()
{
  if(mEventQueue)
  {
    auto n = mEventQueue->elementsAvailable();
 //   std::cout << "processEvents: " << n << " elems, time: " << mVectorStartTime << "\n";

    for(int i=0; i<n; ++i)
    {
      MLControlEvent e = mEventQueue->peek();
      uint64_t eventTimeInVector = e.mTime - mVectorStartTime;
      
      if(eventTimeInVector < kFloatsPerDSPVector)
      {
        processEvent(mEventQueue->pop());
      }
      else
      {
        break; // assuming events are in time order
      }
    }
  }
}


// process one incoming event by making the appropriate changes in state and change lists.
void MLProcInputToSignals::processEvent(const MLControlEvent &eventParam)
{
  MLControlEvent event = eventParam;
  event.mTime -= mVectorStartTime;

  switch(event.mType)
  {
    case MLControlEvent::kNoteOn:
      doNoteOn(event);
      break;
    case MLControlEvent::kNoteOff:
      doNoteOff(event);
      break;
    case MLControlEvent::kNoteUpdate:
      doNoteUpdate(event);
      break;
    case MLControlEvent::kController:
      doController(event);
      break;
    case MLControlEvent::kPitchWheel:
      doPitchWheel(event);
      break;
    case MLControlEvent::kNotePressure:
      doNotePressure(event);
      break;
    case MLControlEvent::kChannelPressure:
      doChannelPressure(event);
      break;
    case MLControlEvent::kSustainPedal:
      doSustain(event);
      break;
    case MLControlEvent::kNull:
    default:
      break;
  }
}

void MLProcInputToSignals::doNoteOn(const MLControlEvent& event)
{
  // find free event or bail
  int freeEventIdx = mNoteEventsPlaying.findFreeEvent();
  if(freeEventIdx < 0) return;
  mNoteEventsPlaying[freeEventIdx] = event;

  if(mUnisonMode)
  {
    // push any event previously occupying voices to pending stack
    // assuming all voices are playing the same event.
    if (mVoices[0].mState == MLVoice::kOn)
    {
      const MLControlEvent& prevEvent = mVoices[0].mCurrentUnisonNoteEvent;
      mNoteEventsPending.push(prevEvent);
      mNoteEventsPlaying.clearEventsMatchingID(prevEvent.mID);
    }
    for (int v = 0; v < mCurrentVoices; ++v)
    {
      mVoices[v].addNoteEvent(event, mScale);
    }
  }
  else
  {
    auto v = findFreeVoice(0, mCurrentVoices);
    if(v >= 0)
    {
      mVoices[v].addNoteEvent(event, mScale);
    }
    else
    {
      // find a sustained voice to steal
      v = findOldestSustainedVoice();

      // or failing that, the voice with the nearest note
      if(v < 0)
      {
        int note = event.mValue1;
        v = findNearestVoice(note);
      }

      // steal it
      mVoices[v].stealNoteEvent(event, mScale, true);
    }
  }
}

void MLProcInputToSignals::doNoteOff(const MLControlEvent& event)
{
  // clear all events matching instigator
  int instigator = event.mID;
  int chan = event.mChannel;
  for (int i=0; i<kMaxEvents; ++i)
  {
    if(mNoteEventsPlaying[i].mID == instigator)
    {
      mNoteEventsPlaying[i].clear();
    }
  }

  if(mUnisonMode)
  {
    // if note off is the sounding event,
    // play the most recent note from pending stack, or release or sustain last note.
    // else delete the note from events and pending stack.
    if(mVoices[0].mInstigatorID == instigator)
    {
      if(!mNoteEventsPending.isEmpty())
      {
        MLControlEvent pendingEvent = mNoteEventsPending.pop();
        for (int v = 0; v < mCurrentVoices; ++v)
        {
          mVoices[v].stealNoteEvent(pendingEvent, mScale, mGlissando);
        }
      }
      else
      {
        // release or sustain
        MLControlEvent::EventType newEventType = mSustainPedal ? MLControlEvent::kNoteSustain : MLControlEvent::kNoteOff;
        for(int v=0; v<mCurrentVoices; ++v)
        {
          MLVoice& voice = mVoices[v];
          MLControlEvent eventToSend = event;
          eventToSend.mType = newEventType;
          voice.addNoteEvent(eventToSend, mScale);
        }
      }
    }
    else
    {
      mNoteEventsPending.clearEventsMatchingID(instigator);
    }
  }
  else // mUnisonMode
  {
    switch(mProtocol)
    {
      case kInputProtocolMIDI:
      case kInputProtocolMIDI_MPE:
      case kInputProtocolOSC:
      {
        // send either off or sustain event to voices matching instigator
        MLControlEvent::EventType newEventType = mSustainPedal ? MLControlEvent::kNoteSustain : MLControlEvent::kNoteOff;

        for(int v=0; v<mCurrentVoices; ++v)
        {
          MLVoice& voice = mVoices[v];
          if((voice.mInstigatorID == instigator) && (voice.mState == MLVoice::kOn))
          {
            MLControlEvent eventToSend = event;
            eventToSend.mType = newEventType;
            voice.addNoteEvent(eventToSend, mScale);
          }
        }
        break;
      }
    }
  }
}

// update multiple axes of control for a held note event.
void MLProcInputToSignals::doNoteUpdate(const MLControlEvent& event)
{
  int instigator = event.mID;
  for(int v=0; v<mCurrentVoices; ++v)
  {
    MLVoice& voice = mVoices[v];
    if((voice.mInstigatorID == instigator) && (voice.mState == MLVoice::kOn))
    {
      mVoices[v].addNoteEvent(event, mScale);
    }
  }
}

void MLProcInputToSignals::doSustain(const MLControlEvent& event)
{
  mSustainPedal = (int)event.mValue1;
  if(!mSustainPedal)
  {
    // clear any sustaining voices
    for(int i=0; i<mCurrentVoices; ++i)
    {
      MLVoice& v = mVoices[i];
      if(v.mState == MLVoice::kSustain)
      {
        MLControlEvent newEvent;
        newEvent.mType = MLControlEvent::kNoteOff;
        v.addNoteEvent(newEvent, mScale);
      }
    }
  }
}

// if the controller number matches one of the numbers we are sending to the
// patcher, update it.
void MLProcInputToSignals::doController(const MLControlEvent& event)
{
  int instigator = event.mID;
  auto time = event.mTime;
  int ctrl = event.mValue1;
  int chan = event.mChannel;
  float val = event.mValue2;
  
  switch(mProtocol)
  {
      // note: this is for MIDI. OSC controller changes are handled through doNoteUpdate()
    case kInputProtocolMIDI:
    {
      if(ctrl == 120)
      {
        if(val == 0)
        {
          // all sound off
          clear();
        }
      }
      else if(ctrl == 123)
      {
        if(val == 0)
        {
          // all notes off
          for(int v=0; v<mCurrentVoices; ++v)
          {
            MLVoice& voice = mVoices[v];
            if(voice.mState != MLVoice::kOff)
            {
              MLControlEvent eventToSend = event;
              eventToSend.mType = MLControlEvent::kNoteOff;
              voice.addNoteEvent(eventToSend, mScale);
            }
          }
        }
      }
      else
      {
        // modulate all voices.
        for (int i=0; i<mCurrentVoices; ++i)
        {
          if(ctrl == mControllerNumber)
            mVoices[i].mdMod.addChange(val, time);
          else if (ctrl == mControllerNumber + 1)
            mVoices[i].mdMod2.addChange(val, time);
          else if (ctrl == mControllerNumber + 2)
            mVoices[i].mdMod3.addChange(val, time);
        }
      }
      break;
    }
    case kInputProtocolMIDI_MPE:
    {
      if(chan == 1) // MPE main voice
      {
        if(ctrl == 120)
        {
          if(val == 0)
          {
            // all sound off
            clear();
          }
        }
        else if(ctrl == 123)
        {
          if(val == 0)
          {
            // all notes off
            for(int v=0; v<mCurrentVoices; ++v)
            {
              MLVoice& voice = mVoices[v];
              if(voice.mState != MLVoice::kOff)
              {
                MLControlEvent eventToSend = event;
                eventToSend.mType = MLControlEvent::kNoteOff;
                voice.addNoteEvent(eventToSend, mScale);
              }
            }
          }
        }
        else
        {
          // TODO add parameter for x cc
          if (ctrl == mControllerMPEXNumber) // x for MPE input, default 73
            mMPEMainVoice.mdMod2.addChange(val, time);
          else if (ctrl == 74) // y always 74
            mMPEMainVoice.mdMod3.addChange(val, time);
          else if(ctrl == mControllerNumber)
          {
            mMPEMainVoice.mdMod.addChange(val, time);
          }
        }
      }
      else // modulate voices matching instigator
      {
        for(int v=0; v<mCurrentVoices; ++v)
        {
          MLVoice& voice = mVoices[v];
          if((voice.mInstigatorID == instigator) && (voice.mState == MLVoice::kOn))
          {
            if (ctrl == mControllerMPEXNumber) // x for MPE input, default 73
              mVoices[v].mdMod2.addChange(val, time);
            else if (ctrl == 74) // y always 74
              mVoices[v].mdMod3.addChange(val, time);
            else if(ctrl == mControllerNumber)
              mVoices[v].mdMod.addChange(val, time);
          }
        }
      }
      break;
    }
    case kInputProtocolOSC:
    {
      // currently unimplemented but will be when we do OSC through events
      break;
    }
  }
}

void MLProcInputToSignals::doPitchWheel(const MLControlEvent& event)
{
  float val = event.mValue1;
  float ctr = val - 8192.f;
  float u = ctr / 8191.f;
  float bendAdd = u * mPitchWheelSemitones / 12.f;
  int chan = event.mChannel;

  switch(mProtocol)
  {
    case kInputProtocolMIDI:
    {
      for (int i=0; i<mCurrentVoices; ++i)
      {
        mVoices[i].mdPitchBend.addChange(bendAdd, event.mTime);
      }
      break;
    }
    case kInputProtocolMIDI_MPE:
    {
      if (chan == 1) // MPE Main Channel
      {
        mMPEMainVoice.mdPitchBend.addChange(bendAdd, event.mTime);
      }
      else
      {
        
        // save bend by channel
        mPitchBendChangesByChannel[chan].addChange(bendAdd, event.mTime);

        // in MPE mode, instigator is channel
        
        // in MPE mode, keep track of each voice's instigator and
        // add bend signals to notes on output
        
        
        /*
        // send pitch to all voices matching instigator
        for (int v=0; v<mCurrentVoices; ++v)
        {
          if ((mVoices[v].mInstigatorID == event.mID) && (mVoices[v].mState == MLVoice::kOn))
          {
            mVoices[v].mdPitchBend.addChange(bendAdd, event.mTime);
          }
        }
        */
        
      }
      break;
    }
    case kInputProtocolOSC:
    {
      // currently unimplemented but will be when we do OSC through events
      break;
    }
  }
}

void MLProcInputToSignals::doNotePressure(const MLControlEvent& event)
{
  switch(mProtocol)
  {
    case kInputProtocolMIDI:
    {
      for (int i=0; i<mCurrentVoices; ++i)
      {
        if (event.mID == mVoices[i].mInstigatorID)
        {
          mVoices[i].mdNotePressure.addChange(event.mValue2, event.mTime);
        }
      }
      break;
    }
    case kInputProtocolMIDI_MPE:    // note pressure is ignored in MPE mode
    {
      break;
    }
    case kInputProtocolOSC:
    {
      // currently unimplemented but will be when we do OSC through events
      break;
    }
  }
}

void MLProcInputToSignals::doChannelPressure(const MLControlEvent& event)
{
  switch(mProtocol)
  {
    case kInputProtocolMIDI:
    {
      for (int i=0; i<mCurrentVoices; ++i)
      {
        mVoices[i].mdChannelPressure.addChange(event.mValue1, event.mTime);
      }
      break;
    }
    case kInputProtocolMIDI_MPE:
    {
      if (event.mChannel == 1) // MPE Main Channel
      {
        mMPEMainVoice.mdChannelPressure.addChange(event.mValue1, event.mTime);
      }
      else
      {
        // send pressure to all active voices matching instigator
        for (int v=0; v<mCurrentVoices; ++v)
        {
          if ((mVoices[v].mInstigatorID == event.mID) && (mVoices[v].mState == MLVoice::kOn))
          {
            mVoices[v].mdChannelPressure.addChange(event.mValue1, event.mTime);
          }
        }
      }
      break;
    }
    case kInputProtocolOSC:
    {
      // currently unimplemented but will be when we do OSC through events
      break;
    }
  }
}

// process change lists to make output signals
//
void MLProcInputToSignals::writeOutputSignals(const int frames)
{
  // get main channel signals for MPE
  if(mProtocol == kInputProtocolMIDI_MPE)
  {
    mMPEMainVoice.mdPitchBend.writeToSignal(mMainPitchSignal, frames);
    mMPEMainVoice.mdChannelPressure.writeToSignal(mMainChannelPressureSignal, frames);
    mMPEMainVoice.mdMod.writeToSignal(mMainModSignal, frames);
    mMPEMainVoice.mdMod2.writeToSignal(mMainMod2Signal, frames);
    mMPEMainVoice.mdMod3.writeToSignal(mMainMod3Signal, frames);
  }
  
  // get pitch bend input channel signals for MPE
  if(mProtocol == kInputProtocolMIDI_MPE)
  {
    for(int i=0; i < kMPEInputChannels; ++i)
    {
      auto& c = mPitchBendChangesByChannel[i];
      c.writeToSignal(mPitchBendSignals[i], frames);
    }
  }
  
  int maxVoices = getContext()->getRootContext()->getMaxVoices();
  for (int v=0; v<maxVoices; ++v)
  {
    // changes per voice
    MLSignal& pitch = getOutput(v*kNumVoiceSignals + 1);
    MLSignal& gate = getOutput(v*kNumVoiceSignals + 2);
    MLSignal& velSig = getOutput(v*kNumVoiceSignals + 3);
    MLSignal& voiceSig = getOutput(v*kNumVoiceSignals + 4);
    MLSignal& after = getOutput(v*kNumVoiceSignals + 5);
    MLSignal& mod = getOutput(v*kNumVoiceSignals + 6);
    MLSignal& mod2 = getOutput(v*kNumVoiceSignals + 7);
    MLSignal& mod3 = getOutput(v*kNumVoiceSignals + 8);

    // write signals
    if (v < mCurrentVoices)
    {
      // write pitch
      mVoices[v].mdPitch.writeToSignal(pitch, frames);
      
    
      // add pitch bend
      if(mProtocol == kInputProtocolMIDI_MPE)
      {
        // from MPE main voice
        pitch.add(mMainPitchSignal);
        
        // from channel that triggered the voice
        if(within(mVoices[v].mChannel, 1, kMPEInputChannels))
        {
          pitch.add(mPitchBendSignals[mVoices[v].mChannel]);
        }
      }
      else if(mProtocol == kInputProtocolMIDI_MPE)
      {
        // add pitch bend in semitones to pitch
        mVoices[v].mdPitchBend.writeToSignal(mTempSignal, frames);
        pitch.add(mTempSignal);
      }

#if INPUT_DRIFT
      // write to common temp drift signal, we add one change manually so read offset is 0
      mVoices[v].mdDrift.writeToSignal(mTempSignal, frames);
      pitch.add(mTempSignal);
#endif

      // write master_tune param offset
      mTempSignal.fill(mMasterPitchOffset);
      pitch.add(mTempSignal);

      mVoices[v].mdGate.writeToSignal(gate, frames);

      // initial velocity output
      mVoices[v].mdVel.writeToSignal(velSig, frames);

      // voice constant output
      voiceSig.setToConstant(v);

      // aftertouch / z output
      switch(mProtocol)
      {
        case kInputProtocolMIDI:
          // add channel aftertouch + poly aftertouch.
          mVoices[v].mdNotePressure.writeToSignal(after, frames);
          mVoices[v].mdChannelPressure.writeToSignal(mTempSignal, frames);
          after.add(mTempSignal);
          break;
        case kInputProtocolMIDI_MPE:
          // MPE ignores poly aftertouch.
          mVoices[v].mdChannelPressure.writeToSignal(after, frames);
          after.add(mMainChannelPressureSignal);
          break;
        case kInputProtocolOSC:
          // write amplitude to aftertouch signal
          mVoices[v].mdAmp.writeToSignal(after, frames);
          break;
      }

      mVoices[v].mdMod.writeToSignal(mod, frames);
      mVoices[v].mdMod2.writeToSignal(mod2, frames);
      mVoices[v].mdMod3.writeToSignal(mod3, frames);

      if(mProtocol == kInputProtocolMIDI_MPE)
      {
        mod.add(mMainModSignal);
        mod2.add(mMainMod2Signal);
        mod3.add(mMainMod3Signal);

        // over MPE, we can make bipolar x and y signals to match the OSC usage.
        // only center the x controller if the controller number being used is the default.
        if(mControllerMPEXNumber == 73)
        {
          mod2.scale(2.0f);
          mod2.add(-1.0f);
        }
        mod3.scale(2.0f);
        mod3.add(-1.0f);
      }

      // clear change lists
      mVoices[v].mdPitch.clearChanges();
      mVoices[v].mdPitchBend.clearChanges();
      mVoices[v].mdGate.clearChanges();
      mVoices[v].mdAmp.clearChanges();
      mVoices[v].mdVel.clearChanges();
      mVoices[v].mdNotePressure.clearChanges();
      mVoices[v].mdChannelPressure.clearChanges();
      mVoices[v].mdMod.clearChanges();
      mVoices[v].mdMod2.clearChanges();
      mVoices[v].mdMod3.clearChanges();
#if INPUT_DRIFT
      mVoices[v].mdDrift.clearChanges();
#endif
    }
    else
    {
      pitch.setToConstant(0.f);
      gate.setToConstant(0.f);
      velSig.setToConstant(0.f);
      voiceSig.setToConstant(0.f);
      after.setToConstant(0.f);
      mod.setToConstant(0.f);
      mod2.setToConstant(0.f);
      mod3.setToConstant(0.f);
    }
  }
}

#pragma mark -

// return index of free voice or -1 for none.
// increments mVoiceRotateOffset.
//
int MLProcInputToSignals::findFreeVoice(size_t start, size_t len)
{
  int r = -1;
  for (auto v = start; v < start + len; ++v)
  {
    auto vr = v;
    if(mRotateMode)
    {
      vr = (vr + mVoiceRotateOffset) % len;
    }
    if (mVoices[vr].mState == MLVoice::kOff)
    {
      r = static_cast<int>(vr);
      mVoiceRotateOffset++;
      break;
    }
  }
  return r;
}

int MLProcInputToSignals::findOldestSustainedVoice()
{
  int r = -1;
  std::list<int> sustainedVoices;
  for (int i=0; i<mCurrentVoices; ++i)
  {
    MLVoice& v = mVoices[i];
    if(v.mState == MLVoice::kSustain)
    {
      sustainedVoices.push_back(i);
    }
  }

  int maxAge = -1;
  for(std::list<int>::const_iterator it = sustainedVoices.begin();
      it != sustainedVoices.end(); it++)
  {
    int voiceIdx = *it;
    int age = mVoices[voiceIdx].mAge;
    if (age > maxAge)
    {
      maxAge = age;
      r = voiceIdx;
    }
  }
  return r;
}

// return the index of the voice with the note nearest to the note n.
// Must always return a valid voice index.
int MLProcInputToSignals::findNearestVoice(int note)
{
  int r = 0;
  int minDist = 128;
  for (int v=0; v<mCurrentVoices; ++v)
  {
    int vNote = mVoices[v].mNote;
    int noteDist = abs(note - vNote);
    if (noteDist < minDist)
    {
      minDist = noteDist;
      r = v;
    }
  }
  return r;
}

// unused
int MLProcInputToSignals::findOldestVoice()
{
  int r = 0;
  int maxAge = -1;
  for (int v=0; v<mCurrentVoices; ++v)
  {
    int age = mVoices[v].mAge;
    if (age > maxAge)
    {
      maxAge = age;
      r = v;
    }
  }
  return r;
}

int MLProcInputToSignals::MPEChannelToVoiceIDX(int i)
{
  return (i - 2) % mCurrentVoices;
}

void MLProcInputToSignals::dumpEvents()
{
  for (int i=0; i<kMaxEvents; ++i)
  {
    MLControlEvent& event = mNoteEventsPlaying[i];
    int type = event.mType;
    switch(type)
    {
      case MLControlEvent::kNull:
        //debug() << "-";
        break;
      case MLControlEvent::kNoteOn:
        //debug() << "N";
        break;
      default:
        //debug() << "?";
        break;
    }
  }
  //debug() << "\n";
  int pendingSize = mNoteEventsPending.getSize();
  //debug() << pendingSize << " events pending: ";
  if(pendingSize > 0)
  {
    for(int i = 0; i < pendingSize; ++i)
    {
      //debug() << mNoteEventsPending[i].mID << " ";
    }
  }
  //debug() << "\n";
}

void MLProcInputToSignals::dumpVoices()
{
  debug() << "voices:\n";
  for (int i=0; i<mCurrentVoices; ++i)
  {
    debug() << "    " << i << ": ";

    MLVoice& voice = mVoices[i];
    debug() << "[i: " << voice.mInstigatorID << "]";
    
    switch(voice.mState)
    {
      case MLVoice::kOff:
        debug() << "off";
        break;
      case MLVoice::kOn:
        debug() << " on";
        break;
      case MLVoice::kSustain:
        debug() << "sus";
        break;
      default:
        debug() << " ? ";
        break;
    }
  debug() << "\n";
    
  }

}

void MLProcInputToSignals::dumpSignals()
{
  debug() << "signals:\n";
  for (int i=0; i<mCurrentVoices; ++i)
  {
    debug() << "    " << i << ": ";

    // changes per voice
    MLSignal& pitch = getOutput(i*kNumVoiceSignals + 1);
    MLSignal& gate = getOutput(i*kNumVoiceSignals + 2);
    MLSignal& vel = getOutput(i*kNumVoiceSignals + 3);
    MLSignal& voice = getOutput(i*kNumVoiceSignals + 4);
    MLSignal& after = getOutput(i*kNumVoiceSignals + 5);

    debug() << "[pitch: " << pitch[0] << "] ";
    debug() << "[gate : " << gate[0] << "] ";
    //debug() << "[vel  : " << vel[0] << "] ";
    //debug() << "[voice: " << voice[0] << "] ";
    debug() << "[after: " << after[0] << "] ";
    //debug() << "\n";
    debug() << "\n";
    
  }

}


void MLProcInputToSignals::dumpTouchFrame()
{

  TouchFrame t = mLatestTouchFrame;

  for (int i=0; i<kMaxTouches; ++i)
  {
    std::cout << "[" << t[i].z << "] ";


    //debug() << "[pitch: " << pitch[0] << "] ";
    //debug() << "[gate : " << gate[0] << "] ";
    //debug() << "[vel  : " << vel[0] << "] ";
    //debug() << "[voice: " << voice[0] << "] ";
    //debug() << "[after: " << after[0] << "] ";
    //debug() << "\n";
  }
  std::cout << "\n";
}


