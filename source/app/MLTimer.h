//
//  MLTimer.h
//  madronalib
//
//  Created by Randy Jones on 9/10/2018
//

#pragma once

#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <set>
#include <mutex>

using namespace std::chrono;

// for use with the JUCE framework: https://github.com/WeAreROLI
// JUCE does not allow many display functions to be called from what it considers the wrong thread,
// so turn this on to start all our Timers from juce's message thread.
#ifdef JUCE_APP_VERSION
#define MADRONALIB_TIMERS_USE_JUCE 1
#endif

#if MADRONALIB_TIMERS_USE_JUCE
#include "JuceHeader.h"
#endif

namespace ml
{
	// A simple, low-resolution timer for doing applicaton and UI tasks.
	// Any callbacks are called synchronously from the main run loop so
	// callbacks should not take too much time. To trigger an action
	// that might take longer, send a message from the callback and
	// then receive it and do the action in a private thread.
	
	class Timer;
	void serviceTimer(Timer* p, time_point<system_clock> now);
	
	constexpr int kMillisecondsResolution = 16;

	
	class Timers
#if MADRONALIB_TIMERS_USE_JUCE
	: private juce::Timer
#endif
	
	{
		
	public:
		
#if MADRONALIB_TIMERS_USE_JUCE
		Timers() { startTimer(kMillisecondsResolution); }
		~Timers() { running = false; stopTimer(); }
#else
		Timers() { }
		~Timers() { running = false; runThread.join(); }
#endif
		
		// singleton: we only want one Timers instance. The first time a Timer object is made,
		// this object is made and the run thread is started.
		static Timers &theTimers()  { static Timers t; return t; }
		
		// delete copy and move constructors and assign operators
		Timers(Timers const&) = delete;             // Copy construct
		Timers(Timers&&) = delete;                  // Move construct
		Timers& operator=(Timers const&) = delete;  // Copy assign
		Timers& operator=(Timers &&) = delete;      // Move assign
		
		void insert(ml::Timer* t)
		{
			timerPtrs.insert(t);
		}
		
		void erase(ml::Timer* t)
		{
			timerPtrs.erase(t);
		}
		
		std::mutex mSetMutex;
		
	private:
		bool running { true };
		std::set< ml::Timer* > timerPtrs;
		
#if MADRONALIB_TIMERS_USE_JUCE
		void timerCallback()
		{
			runNow();
		}
#else
		std::thread runThread { [&](){ run(); } };
#endif
		
		void runNow(void)
		{
			time_point<system_clock> now = system_clock::now();
			std::unique_lock<std::mutex> lock(mSetMutex);
			for(auto t : timerPtrs)
			{
				serviceTimer(t, now);
			}
		}
		
		void run(void)
		{
			while(running)
			{
				std::this_thread::sleep_for(milliseconds(kMillisecondsResolution));
				runNow();
			}
		}
		
	}; // class Timers
	
	
	
	class Timer
	{
		friend class Timers;
		friend void serviceTimer(Timer* t, time_point<system_clock> now);
		
	public:
		
		Timer() noexcept
		{
			Timers& t{Timers::theTimers()};
			std::unique_lock<std::mutex> lock(t.mSetMutex);
			Timers::theTimers().insert(this);
		}
		
		~Timer()
		{
			std::unique_lock<std::mutex> lock(Timers::theTimers().mSetMutex);
			Timers::theTimers().erase(this);
		}
		
		// delete copy and move constructors and assign operators
		Timer(Timer const&) = delete;             // Copy construct
		Timer(Timer&&) = delete;                  // Move construct
		Timer& operator=(Timer const&) = delete;  // Copy assign
		Timer& operator=(Timer &&) = delete;      // Move assign
		
		// call the function once after the specified interval.
		void callOnce(std::function<void(void)> f, const milliseconds period)
		{
			mCounter = 1;
			myFunc = f;
			mPeriod = period;
			mPreviousCall = system_clock::now();
		}
		
		// call the function n times, waiting the specified interval before each.
		void callNTimes(std::function<void(void)> f, const milliseconds period, int n)
		{
			mCounter = n;
			myFunc = f;
			mPeriod = period;
			mPreviousCall = system_clock::now();
		}
		
		// start calling the function periodically. the wait period happens before the first call.
		void start(std::function<void(void)> f, const milliseconds period)
		{
			mCounter = -1;
			myFunc = f;
			mPeriod = period;
			mPreviousCall = system_clock::now();
		}
		
		bool isActive()
		{
			return mCounter != 0;
		}
		
		void stop()
		{
			// More lightweight ways of handling a race on mCounter are possible, but as
			// stopping a timer is an infrequent operation we use the mutex for laziness and brevity.
			std::unique_lock<std::mutex> lock(Timers::theTimers().mSetMutex);
			mCounter = 0;
		}
		
	private:
		int mCounter{0};
		std::function<void(void)> myFunc;
		milliseconds mPeriod;
		time_point<system_clock> mPreviousCall;
	};
	
	
	inline void serviceTimer(Timer* t, time_point<system_clock> now)
	{
		if(t->mCounter != 0)
		{
			if(now - t->mPreviousCall > t->mPeriod)
			{
				t->myFunc();
				if(t->mCounter > 0)
				{
					t->mCounter--;
				}
				t->mPreviousCall = now;
			}
		}
	}

	
} // namespace ml


