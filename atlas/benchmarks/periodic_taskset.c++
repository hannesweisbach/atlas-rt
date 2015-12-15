#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

#include <cstdlib>
#include <cstring>

#include <boost/math/common_factor_rt.hpp>

#include "atlas/atlas.h"
#include "common/common.h"

#include "taskgen.h"

static auto hyperperiod(const std::vector<task_attr> &tasks) {
  auto counts = std::accumulate(
      std::begin(tasks), std::end(tasks), hyperperiod_t::rep{1},
      [](const auto &hp, const auto &task) {
        return boost::math::lcm(
            hp, std::chrono::duration_cast<hyperperiod_t>(task.p).count());
      });

  return hyperperiod_t{counts};
}

static auto do_work(const execution_time &e) {
  using namespace std::chrono;
  static constexpr execution_time work_unit{100us};
  static constexpr execution_time small_work_unit{10us};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#if 1
  static std::string haystack(39 * 32 * 1024, 'A');
  static std::string small_haystack(33 * 8 * 512, 'A');
#else
  static std::string haystack(25 * 512, 'A');
  static std::string small_haystack(128, 'A');
#endif
#pragma clang diagnostic pop

  auto start = cputime_clock::now();

  for (size_t i = 0; i < static_cast<size_t>(e / work_unit); ++i) {
    if (reset_deadline())
      return EXIT_FAILURE;
    if (strstr(haystack.c_str(), "test"))
      std::cout << "Found." << std::endl;
  }

  const auto remaining = e - (cputime_clock::now() - start);
  if (remaining <= 0s)
    return EXIT_SUCCESS;

  for (size_t i = 0; i < static_cast<size_t>(remaining / small_work_unit);
       ++i) {
    if (reset_deadline())
      return EXIT_FAILURE;
    if (strstr(small_haystack.c_str(), "test"))
      std::cout << "Found." << std::endl;
  }

#if 0
  const auto end = cputime_clock::now();
  const auto diff = end - start;
  std::cout << duration_cast<microseconds>(diff).count() << "us ";
  std::cout << static_cast<size_t>(e / work_unit) << " ";
  std::cout << static_cast<size_t>(remaining / small_work_unit) << " ";
  std::cout << std::endl;
#endif
#if 1
  for (; small_work_unit < e - (cputime_clock::now() - start);) {
    if (reset_deadline())
      return EXIT_FAILURE;
    if (strstr(small_haystack.c_str(), "test"))
      std::cout << "Found." << std::endl;
  }
#if 1
#endif
#endif

  return EXIT_SUCCESS;
}

class periodic_taskset {
  struct task {
    task_attr attr;
    mutable std::atomic_bool done{false};
  };

  std::vector<task> tasks;
  std::unique_ptr<std::thread[]> threads;

  hyperperiod_t hyperperiod;
  std::atomic_bool deadline_miss{false};
  std::atomic_bool stop{false};

  auto run(const size_t i) {
    const task &task = tasks.at(i);
    const auto jobs = hyperperiod / task.attr.p;
    record_deadline_misses();

    for (auto job = 0; job < jobs && !deadline_miss && !stop; ++job) {
      uint64_t id;
      atlas::next(id);

      if (do_work(task.attr.e) == EXIT_FAILURE) {
        std::cout << "Task " << task.attr << "missed deadline " << job
                  << std::endl;
        deadline_miss = true;
      }
    }

    task.done = true;
  }

  void synchronize_end() {
    using namespace std::chrono;
    stop = true;
    for (size_t i = 0; i < tasks.size(); ++i) {
      if (!tasks.at(i).done) {
        atlas::np::submit(threads[i], 0, 1s, 2s);
      }
      if (threads[i].joinable()) {
        threads[i].join();
      }
    }
  }

public:
  periodic_taskset(const size_t n, U usum, U umax, period p_min, period p_max)
      : tasks(n), threads(std::make_unique<std::thread[]>(n)) {
    const auto attr = generate_taskset(n, usum, umax, p_min, p_max);
    hyperperiod = ::hyperperiod(attr);
    for (size_t i = 0; i < n; ++i) {
      tasks.at(i).attr = attr.at(i);
    }

    for (size_t i = 0; i < n; ++i) {
      threads[i] = std::thread(&periodic_taskset::run, this, i);
    }

  }

  void schedule() {}
};

int main() {
  using namespace std::chrono;
  periodic_taskset ts(4, U{3500}, {1000}, 5ms, 100ms);
}
