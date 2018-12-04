
//
// timerTest
// a unit test made using the Catch framework in catch.hpp / tests.cpp.
//

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>
#include <thread>

#include "catch.hpp"
#include "../include/madronalib.h"
#include "MLTimer.h"

using namespace ml;
using namespace std::chrono;

float workResult{0};
RandomScalarSource randSource;

void lotsOfWork()
{
	float w = 0.f;
	for(int i=0; i < 100000; ++i)
	{
		w += randSource.getFloat();
	}
	std::cout << " lots of work: " << w << " ";
	workResult += w;
}


TEST_CASE("madronalib/core/timer/basic", "[timer][basic]")
{
/*
	int sum = 0;
	
	// test number of calls correct
	const int testSize = 10;
	std::vector< std::unique_ptr< Timer > > v;
	for(int i=0; i<testSize; ++i)
	{
		v.emplace_back( std::unique_ptr< Timer > (new Timer));
		v[i]->callNTimes([&sum](){sum += 1; std::cout << ".";}, milliseconds(100 + 20*i), 2);
	}
	std::this_thread::sleep_for(milliseconds(1000));
	std::cout << "timer sum: " << sum << "\n";
	REQUIRE(sum == 20);
	
	// test deleting timers while running (no REQUIRE)
	std::cout << "delete test:\n";
	const int test2Size = 10;
	{		
		std::vector< std::unique_ptr< Timer > > v2;
		for(int i=0; i<test2Size; ++i)
		{
			v2.emplace_back( std::unique_ptr< Timer > (new Timer));
			v2[i]->start([=](){lotsOfWork(); std::cout << workResult << " ";}, milliseconds(10*i));
		}
		std::this_thread::sleep_for(milliseconds(100));
		for(int i=0; i<test2Size; ++i)
		{
			std::cout << "erasing: ";
			v2.erase( v2.begin() );
			std::this_thread::sleep_for(milliseconds(100));
		}
	}

	
	// test stopping timers while running
	const int test3Size = 10;
	{
		int sum{0};
		std::cout << "\n----\n";
		std::vector< std::unique_ptr< Timer > > v3;
		for(int i=0; i<test3Size; ++i)
		{
			v3.emplace_back( std::unique_ptr< Timer > (new Timer));
			v3[i]->start([&sum, i](){sum++; std::cout << i << " ";}, milliseconds(10));
		}
		for(int i=0; i<test3Size; ++i)
		{
			std::this_thread::sleep_for(milliseconds(10));
			v3[i]->stop();
		}
		
		// make sure all timers have stopped
		std::this_thread::sleep_for(milliseconds(100));
		sum = 0;
		std::this_thread::sleep_for(milliseconds(100));
		REQUIRE(sum == 0);
	}
	
	*/
	
	std::cout << "\n\n\ndelete test 2:\n";
	Timer* t3;
	t3 = new Timer();
	t3->start([=](){lotsOfWork(); std::cout << "* " << workResult << " \n";}, milliseconds(1));
	//std::this_thread::sleep_for(milliseconds(100));
	std::cout << "\n started \n";
	std::this_thread::sleep_for(milliseconds(100));
	std::cout << "\n deleting... \n";
	delete(t3);
	t3 = nullptr;
	
// TODO store like everything in the Timers object. Just keep an ID to send the Timers on deletion. 
	// on Timers closing, just leaking is fine
	
	// Tiemrs still has one delete flag per Timer ID to avoid enququeing issues
	
#ifdef _WINDOWS
	system("pause");
#endif

}
