#include <chrono>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <list>
#include <thread>
#include <vector>

#include "atlas/atlas.h"
#include "runtime/cputime_clock.h"

using namespace std::chrono;
using execution_time = std::chrono::nanoseconds;

static auto do_work(const execution_time &e) {
  static constexpr execution_time work_unit{100us};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
  static std::string haystack(39 * 32 * 1024, 'A');
#pragma clang diagnostic pop

  if (e <= execution_time{0}) {
    return;
  }

  const auto start = cputime_clock::now();

  while (cputime_clock::now() - start < e) {
    strstr(haystack.c_str(), "test");
  }
}

struct datapoint {
  int group;
  uint64_t id;
  nanoseconds exectime;
  atlas::clock::time_point deadline;
  atlas::clock::time_point completion;
  nanoseconds tardiness;
};

static auto threadfn(const int group, const uint64_t myid, const atlas::clock::time_point gotime,
                     const atlas::clock::time_point deadline,
                     const execution_time e) {
  std::this_thread::sleep_until(gotime);

  uint64_t id;
  if (atlas::next(id) != 1) {
    throw std::runtime_error("Unexected return value from atlas::next.");
  }
#if 0
  if (id != myid) {
    throw std::runtime_error("Got wrong id.");
  }
#endif
  do_work(e);
  const auto now = atlas::clock::now();
  if (atlas::next(id) != 0) {
    throw std::runtime_error("Unexpected return value from next.");
  }

  return datapoint{group, id, e, deadline, now, now - deadline};
}

int main() {
  std::vector<datapoint> measurement;
  std::list<std::future<datapoint>> results;
  std::list<std::thread> threads;

  const auto gotime = atlas::clock::now() + 2s;
  constexpr auto max_jobs = 25;
  measurement.reserve(max_jobs);

  for(int i = 0; i < max_jobs; ++i) {
    const auto deadline = gotime + (i + 1) * 100ms;
    std::packaged_task<datapoint(int, uint64_t, atlas::clock::time_point, atlas::clock::time_point, nanoseconds)> task1(threadfn);
    std::packaged_task<datapoint(int, uint64_t, atlas::clock::time_point, atlas::clock::time_point, nanoseconds)> task2(threadfn);
    results.emplace_back(task1.get_future());
    results.emplace_back(task2.get_future());
    constexpr auto exec1 = 90ms;
    constexpr auto exec2 = 5ms;
    threads.emplace_back(std::move(task1), 1, i, gotime, deadline, exec1);
    atlas::np::submit(threads.back(), 100+i, exec1 * 1.025, deadline);
    threads.emplace_back(std::move(task2), 2, i, gotime, deadline, exec2);
    atlas::np::submit(threads.back(), 200+i, exec2 * 1.025, deadline);
  }

  for (auto &&t : threads) {
    t.join();
  }

  for (auto &&f : results) {
    measurement.push_back(f.get());
  }

  for (const auto &m : measurement) {
    std::cout << m.group << " " << std::setw(2) << m.id << " "
              << std::setw(4) << duration_cast<milliseconds>(m.exectime).count() << " "
              << std::setw(4) << duration_cast<milliseconds>(m.deadline - gotime).count() << " "
              << std::setw(4) << duration_cast<milliseconds>(m.completion - gotime).count() << " "
              << duration_cast<milliseconds>(m.tardiness).count() << std::endl;
  }
}
