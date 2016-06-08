#pragma once

#include <chrono>
#include <future>
#include <string>

namespace atlas {
struct work_item {
  std::chrono::steady_clock::time_point deadline;
  std::chrono::microseconds prediction;
  const double *metrics;
  size_t metrics_count;
  uint64_t type;
  std::packaged_task<void()> work;
  bool is_realtime;
};

#ifdef HAVE_GCD
std::unique_ptr<executor> make_gcd_queue(const std::string&);
#endif
}
