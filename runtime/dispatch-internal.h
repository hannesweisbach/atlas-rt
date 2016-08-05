#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <list>
#include <mutex>
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

class executor {
  mutable std::condition_variable empty;
  mutable std::mutex list_lock;
  mutable std::list<work_item> work_queue;

  virtual void
  submit(const uint64_t id, const std::chrono::nanoseconds exectime,
         const std::chrono::steady_clock::time_point deadline) const = 0;
  mutable std::atomic_bool done{false};

protected:
  void shutdown() const;
  void work_loop() const;

public:
  virtual ~executor();
  virtual void enqueue(work_item work) const;
};

#ifdef HAVE_GCD
class executor;
std::unique_ptr<executor> make_gcd_queue(const std::string&);
#endif
}
