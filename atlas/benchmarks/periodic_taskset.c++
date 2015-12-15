#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <sched.h>

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
    pid_t tid;
    mutable std::atomic_bool init{false};
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

    task.init = true;
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
      tasks.at(i).tid = atlas::np::from(threads[i]);
    }
  }

  bool simulate() {
    using namespace std::chrono;
    struct release {
      steady_clock::time_point r;
      execution_time e;
      period p;
      pid_t tid;
      size_t count;

      bool operator<(const release &rhs) { return r < rhs.r; }
    };

#if 0
    std::cout << "Running simulation for "
              << duration_cast<s>(hyperperiod).count() << "s" << std::endl;
#endif

    {
      struct sched_param param;
      param.sched_priority = sched_get_priority_max(SCHED_FIFO);
      if (sched_setscheduler(0, SCHED_FIFO, &param)) {
        std::cerr << "Error setting scheduler (" << errno
                  << "): " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
      }
    }
    const auto t0 = steady_clock::now();

    std::vector<release> releases(tasks.size());
    for (size_t i = 0; i < releases.size(); ++i) {
      auto &release = releases.at(i);
      auto &task = tasks.at(i);

      release.r = t0;
      release.e = task.attr.e;
      release.p = task.attr.p;
      release.tid = atlas::np::from(threads[i]);
    }

    for (; steady_clock::now() <= t0 + hyperperiod;) {
      for (auto &&release : releases) {
        if (release.r <= steady_clock::now()) {
          atlas::submit(release.tid, release.count, release.e,
                        release.r + release.p);
          ++release.count;
          release.r += release.p;
        }
      }

      std::sort(std::begin(releases), std::end(releases));
      if (deadline_miss) {
        struct sched_param param;
        param.sched_priority = sched_get_priority_max(SCHED_OTHER);
        if (sched_setscheduler(0, SCHED_OTHER, &param)) {
          std::cerr << "Error setting scheduler (" << errno
                    << "): " << strerror(errno) << std::endl;
          exit(EXIT_FAILURE);
        }
        break;
      }
      std::this_thread::sleep_until(releases.front().r);
    }

    synchronize_end();
    return deadline_miss;
  }

};

int main() {
  using namespace std::chrono;
  periodic_taskset ts(4, U{3500}, {1000}, 5ms, 100ms);
}
