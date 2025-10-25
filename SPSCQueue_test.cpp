//this makes it so that it doesn't include
//lesser known headers from windows.h that aren't used as often, just speeds up compliation time
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include "rigtorp/SPSCQueue.h"
#include <thread>
#include <chrono>
#include <atomic>

typedef struct dataPack {
	int keep_running; 
};


int keep_running = 1;
int count = 0;

int do_work() {
	return 1;
}

int real_time_loop(rigtorp::SPSCQueue<dataPack>& q) {

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
	std::chrono::nanoseconds worst_case_jitter = std::chrono::nanoseconds(0);
	int worst_cycle = 0;
	dataPack data = *q.front();
	q.pop();

	while (data.keep_running) {
		count++;
		next_wake_time += period;

		//this is where we will get the function that we want to run 
		//from the scsp function handler or state enum
		int test = do_work();
		//spin wait - pins cpu to maximally avoid jitter
		while (std::chrono::steady_clock::now() < next_wake_time) {
			//if we had any makeup time then we yield to os to attempt to avoid jitter from forced outages
			//std::this_thread::yield();
		}
		auto error = std::chrono::steady_clock::now() - next_wake_time;
		if (error > worst_case_jitter) {
			worst_case_jitter = error;
			worst_cycle = count;
		}
		//std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(error).count() << std::endl;
		running_error = running_error + error;

		//if front is null there is no new state, because this 
		//while loop has the local variables from the thread setup
		//it retains what the previous data was from before
		if (auto newState = q.front()) {
			data = *newState;
			q.pop();
		}
	}
	auto end_time = std::chrono::steady_clock::now();
	auto total_duration = end_time - (next_wake_time - period * count);
	auto average_duration = total_duration / count;
	std::cout << "Average loop duration: " << std::chrono::duration_cast<std::chrono::microseconds>(average_duration).count() << " microseconds\n" << std::endl;
	std::cout << "Total running error: " << std::chrono::duration_cast<std::chrono::microseconds>(running_error).count() << " microseconds\n" << std::endl;
	std::cout << "Average/Amortized running error: " << std::chrono::duration_cast<std::chrono::nanoseconds>(running_error).count() / count << " nanoseconds\n" << std::endl;
	std::cout << "Worst case jitter: " << std::chrono::duration_cast<std::chrono::microseconds>(worst_case_jitter).count() << " microseconds\n" << std::endl;
	std::cout << "Occurred at cycle: " << worst_cycle << "\n" << std::endl;
	std::cout << "real time thread has exited" << std::endl;
	std::cout << "count is " << count << std::endl;
	return 1;
	//for this test the total accumulated error was 10 microseconds over 1000 iterations, 
	//this is 0.01 microseconds per iteration or 329 microseconds for a different iteration
	// which is .329 microseconds per iteration on average
}



int main()
{
	if (SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS) == 0) {
		std::cerr << "Warning: Failed to set process priority." << std::endl;
		std::cerr << "         May need to run as Administrator." << std::endl;
	}
	std::cout << "Hello World!\n";
	rigtorp::SPSCQueue<dataPack> q(1);
	q.push({ 1 });
	//scsp has a flag that makes it so you CANNOT copy it under any circumstances 
	//but std::thread also copies its arguments no matter what function's signature is 
	//so when we tried to pass just q, and in real_time loop, recieve as &q 
	//it failed because it was still copies, throwing an error from scsp, even if it did work 
	//we'd be referencing different q's
	std::thread rt_thread(real_time_loop, std::ref(q));
	std::cout << "main thread is sleeping for 20 s" << std::endl;
	Sleep(20000);
	std::cout << "main thread woke up and is sending stop signal to real time thread" << std::endl;
	q.push({ 0 });

	//then here, because the main thread has done what is needed, and has pushed the end signal 
	//we can now block in the main thread waiting for the real time thread to finish.
	rt_thread.join();
}


