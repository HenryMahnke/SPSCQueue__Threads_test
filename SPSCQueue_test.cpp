// SPSCQueue_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
//this makes it so that it doesn't include
//lesser known headers from windows.h that aren't used as often, just speeds up compliation time
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include "rigtorp/SPSCQueue.h"
#include <thread>
#include <chrono>
#include <atomic>


int keep_running = 1;
int count = 0;

int do_work() {
	return 1;
}

int real_time_loop() {


	//std::this_thread::sleep_until(next_wake_time);

	//get crrent thread 
	HANDLE this_thread = GetCurrentThread();
	DWORD_PTR affinity_mask = (1 << 3);
	if (SetThreadAffinityMask(this_thread, affinity_mask) == 0) {
		std::cerr << "Failed to set thread affinity. Error: " << GetLastError() << std::endl;
	}

	if (SetThreadPriority(this_thread, THREAD_PRIORITY_TIME_CRITICAL) == 0) {
		std::cerr << "Failed to set thread priority. Error: " << GetLastError() << std::endl;
	}
	auto period = std::chrono::microseconds(2000);
	auto next_wake_time = std::chrono::steady_clock::now();
	std::chrono::nanoseconds running_error = std::chrono::nanoseconds(0);
	while (keep_running) {
		count++;
		next_wake_time += period;

		//this is where we will get the function that we want to run 
		//from the scsp function handler or state enum
		int test = do_work();
		//this just spin waiting
		while (std::chrono::steady_clock::now() < next_wake_time) {
		}
		auto error = std::chrono::steady_clock::now() - next_wake_time;
		//std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(error).count() << std::endl;
		running_error = running_error + error;
		if (count == 1000) {
			keep_running = 0;
		}
	}
	auto end_time = std::chrono::steady_clock::now();
	auto total_duration = end_time - (next_wake_time - period * count);
	auto average_duration = total_duration / count;
	std::cout << "Average loop duration: " << std::chrono::duration_cast<std::chrono::microseconds>(average_duration).count() << " microseconds\n" << std::endl;
	std::cout << "Total running error: " << std::chrono::duration_cast<std::chrono::microseconds>(running_error).count() << " microseconds\n" << std::endl;
	return 1;
	//for this test the total accumulated error was 10 microseconds over 1000 iterations, 
	//this is 0.01 microseconds per iteration or 329 microseconds for a different iteration
	// which is .329 microseconds per iteration on average
}



int main()
{
	std::cout << "Hello World!\n";
	std::thread rt_thread(real_time_loop);


	rt_thread.join();
}


