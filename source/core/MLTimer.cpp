//
//  MLTimer.cpp
//  madronalib
//
//  Created by Randy Jones on 9/10/2018
//

#include "MLTimer.h"

#include <array>

namespace ml
{
	namespace Time
	{
		const int kMillisecondsResolution{10};
		//const int kMaxTimers{256};
	}

	class Timers
	{
	public:
		Timers() { }
		~Timers() { running = false; theRunThread.join(); }
		
		// singleton: we only want one Timers instance. The first time a Timer object is made,
		// this object is made and the run thread is started.
		static Timers &theTimers()  { static Timers t; return t; }

		// delete copy and move constructors and assign operators
		Timers(Timers const&) = delete;             // Copy construct
		Timers(Timers&&) = delete;                  // Move construct
		Timers& operator=(Timers const&) = delete;  // Copy assign
		Timers& operator=(Timers &&) = delete;      // Move assign
		
		void insert(Timer* t)
		{			
			// vector::push_back is OK while running, the race is benign
			mTimerPtrs.emplace_back(t);
			mDeleteFlags.push_back(false);
		}
		
		void markForDeletion(Timer* t)
		{
			int n = mTimerPtrs.size();
			for(int i=0; i<n; ++i)
			{
				if(t == mTimerPtrs[i])
				{
					mDeleteFlags[i] = true;
					break;
				}
			}
		}
		
	private:
		
//		std::array< Timer*, Time::kMaxTimers > mTimerPtrs;
		std::vector< Timer* > mTimerPtrs;
		std::vector< bool > mDeleteFlags;
		
		int test1{0};
		
		bool running { true };
		std::thread theRunThread { [&](){ run(); } };
		void run(void)
		{
			while(running)
			{
				//mTimerPtrs.fill(nullptr);
				
				time_point<system_clock> now = system_clock::now();
				{
					// call timers
					//std::cout << " size: " << mTimerPtrs.size() << " ";
					// test1 += mTimerPtrs.size();
					int c = 0;
					for(auto t : mTimerPtrs)
					{
					//	Timer* t = *it;
					//	if(t)
						{
							if(!mDeleteFlags[c])
							{
								// we use try_lock here because we don't want to block the whole
								// run loop if one timer mutex is unavailable 
								if(t->mMutex.try_lock())
								{
									if(t->mCounter != 0) // TODO atomic
									{
										if((now - t->mPreviousCall > t->mPeriod) || (t->mPeriod == milliseconds(0)))
										{
											// release-acquire ordering
									//		std::cout << "enter ";
									//		t->mCallingFromRunThread.store(true, std::memory_order_release);
											t->myFunc();									
									//		t->mCallingFromRunThread.store(false, std::memory_order_release);
									//		std::cout << "exit ";
											if(t->mCounter > 0)
											{
												t->mCounter--;
											}
											t->mPreviousCall = now;	
										}
									}
									
									// still crashing here with Assertion failed: (ec == 0), function unlock, file /BuildRoot/Library/Caches/com.apple.xbs/Sources/libcxx/libcxx-400.9/src/mutex.cpp, line 48.
  
									t->mMutex.unlock();
								}
							}
						}
						std::cout << " [" << c << "] ";
						c++;

					}
					
					// TODO subtract time spent in Timer fns
					std::this_thread::sleep_for(milliseconds(Time::kMillisecondsResolution));
					
					// delete any marked	pointers (TODO use low bits for flags)			
					for(int i=0; i<mTimerPtrs.size(); ) // no increment
					{
						if(mDeleteFlags[i])
						{
							mTimerPtrs.erase(mTimerPtrs.begin() + i);
							mDeleteFlags.erase(mDeleteFlags.begin() + i);							
						}
						else
						{
							i++;
						}
					}
				}
			}
		}
	};
		
	Timer::Timer() noexcept : test(3123)
	{
		Timers::theTimers().insert(this);
	}
	
	Timer::~Timer()
	{
		stop();
		
		Timers::theTimers().markForDeletion(this);
		
		// if in run thread, this destructor is being called by our own myFunc() and so waiting would deadlock
		bool inRunThread = mCallingFromRunThread.load(std::memory_order_acquire);
		if(inRunThread) 
		{
			std::cout << "delete in RUN thread \n";
			return;
		}
		
		std::cout << "Timer delete: \n";
		while(!mMutex.try_lock())
		{
			// presumably we are being called by the run thread, so wait here before deletion.
			//std::this_thread::sleep_for(milliseconds(Time::kMillisecondsResolution));
			std::cout << "locked on delete!\n";
		}
		
		// must unlock the mutex before deletion.
		mMutex.unlock();
	}
	
	void Timer::stop()
	{
		// More lightweight ways of handling a race on mCounter are possible, but as
		// stopping a timer is an infrequent operation we use the mutex for brevity.		
		
		// TODO
		
		// std::unique_lock<std::mutex> lock(Timers::theTimers().mSetMutex);
		// mutex prevents the run thread from working with a partially initialized Timer
		//std::lock_guard<std::mutex> lock(mMutex);

		mCounter = 0;
	}

} // namespace ml
