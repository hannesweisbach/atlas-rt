#include <iostream>
#include <chrono>
#include <string>
#include <atomic>

#include "runtime/dispatch.h"

int frameNumber = 1;
double metric = 1;
atlas::dispatch_queue* dispatchQueue;
std::mutex mutex;
std::condition_variable terminateCondition;
std::atomic_bool shouldStop;

double getCPUTime()
{
  timespec ts;
  ::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return ((double) ts.tv_sec)*1000.0 + ((double) ts.tv_nsec) / 1000.0 / 1000.0;
}

void runFrame();

void startFrame() {
  std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20); // 20 ms period
  dispatchQueue->dispatch_async_atlas(deadline, &metric, 1, &runFrame);
}

void runFrame() {
  std::cout << frameNumber++ << "\r";
  std::cout.flush();
  
  // give the thread some artificial work!
  // IMPORTANT! If the thread doesn't have to do anything, the issue cannot be reproduced
  const double time0 = getCPUTime();
  while(getCPUTime() - time0 < 9.0); // 9 ms runtime
  
  // stop on request or launch next frame
  if(shouldStop) {
    std::lock_guard<std::mutex> lock(mutex);
    terminateCondition.notify_one();
  } else {
    startFrame();
  }
}

int main(int argc, char **argv) {
  // create dispatch queue
  dispatchQueue = new atlas::dispatch_queue("atlastest");
  shouldStop = false;
  
  // start atlas thread & wait for user input
  startFrame();
  char a;
  std::cin >> a;
  
  // stop atlas thread and wait for join
  std::cout << "terminate requested..." << std::endl;
  shouldStop = true;
  std::unique_lock<std::mutex> lock(mutex);
  terminateCondition.wait(lock);
  
  // IMPORTANT! When terminating directly, the issue cannot be reproduced.
  // The dispatch queue has to be kept alive without executing an atlas task for some time
  std::cout << "sleeping, then terminating..." << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  
  // terminate
  delete dispatchQueue;
  return 0;
}

