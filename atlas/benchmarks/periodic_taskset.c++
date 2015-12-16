#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <sched.h>

#include <boost/math/common_factor_rt.hpp>
#include <boost/program_options.hpp>

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

  if (e <= execution_time{0}) {
    return;
  }

  auto start = cputime_clock::now();

  for (size_t i = 0; i < static_cast<size_t>(e / work_unit); ++i) {
    if (strstr(haystack.c_str(), "test"))
      std::cerr << "Found." << std::endl;
  }

  const auto remaining = e - (cputime_clock::now() - start);
  if (remaining <= 0s)
    return;

  for (size_t i = 0; i < static_cast<size_t>(remaining / small_work_unit);
       ++i) {
    if (strstr(small_haystack.c_str(), "test"))
      std::cerr << "Found." << std::endl;
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
    if (strstr(small_haystack.c_str(), "test"))
      std::cerr << "Found." << std::endl;
  }
#if 1
#endif
#endif
}

class periodic_taskset {
  struct task {
    task_attr attr;
    pid_t tid;
    mutable std::atomic_bool init{false};
    mutable std::atomic_bool done{false};
    mutable std::atomic<uint64_t> deadline_misses{0};
  };

  std::vector<task> tasks;
  std::unique_ptr<std::thread[]> threads;

  hyperperiod_t hyperperiod;
  std::atomic_bool stop{false};

  auto run(const size_t i) {
    const task &task = tasks.at(i);
    const auto jobs = hyperperiod / task.attr.p;
    record_deadline_misses();

    task.init = true;
    for (auto job = 0; job < jobs && !stop; ++job) {
      uint64_t id;
      atlas::next(id);

      using namespace std::chrono;
#if 1
      do_work(task.attr.e - 500us);
#else
      /* activate for minimum execution time mode */
      do_work(1ms);
#endif
      if (reset_deadline()) {
        ++task.deadline_misses;
        std::cerr << "Task " << task.attr << " failed on job " << job << "/"
                  << jobs << "." << std::endl;
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
      std::this_thread::sleep_until(releases.front().r);
    }

    synchronize_end();
    return std::accumulate(std::begin(tasks), std::end(tasks), 0UL,
                           [](const auto &sum, const auto &task) {
                             return sum + task.deadline_misses;
                           });
  }

  friend std::ostream &operator<<(std::ostream &os,
                                  const periodic_taskset &ts) {
    for (const auto &task : ts.tasks) {
      os << task.attr << std::endl;
    }
    return os;
  }
};

static void find_minimum_e(const size_t count, const period p) {
  using namespace std::chrono;
  std::vector<std::pair<U, size_t>> data;

  set_procfsparam(attribute::preroll, 0);

  for (auto u = U{100}; u < U{1000}; ++u) {
    std::cerr << std::setw(6) << u;
    std::cerr.flush();
    data.emplace_back(u, 0);
    for (size_t j = 0; j < count; ++j) {
      periodic_taskset ts(1, u, u, p, p);
      if (ts.simulate()) {
        ++data.back().second;
      }
    }
    std::cerr << std::setw(5) << data.back().second << std::endl;
  }

  std::ofstream file("data");
  for (const auto &v : data) {
    file << std::setw(8) << v.first << " " << std::setw(8) << v.second
         << std::endl;
  }
}

static void duration(const size_t tasks, const U u_sum, const U u_max,
                     const size_t count, const period pmin, const period pmax,
                     const hyperperiod_t limit) {
  using namespace std::chrono;

  hyperperiod_t duration{0};
  for (size_t j = 0; j < count; ++j) {
    const auto attr = generate_taskset(tasks, u_sum, u_max, pmin, pmax);
    const auto hp = ::hyperperiod(attr);
    if (limit != hyperperiod_t{0} && hp > limit) {
      --j;
      continue;
    }
    duration += hp;
  }

  std::cout << duration_cast<s>(duration).count() << std::endl;
}

static size_t schedulable(const size_t tasks, const U u_sum, const U u_max,
                          const size_t count, const period pmin,
                          const period pmax, const bool preroll = true) {
  using namespace std::chrono;
  size_t failures = 0;
  set_procfsparam(attribute::preroll, preroll);

  for (size_t j = 0; j < count; ++j) {
    periodic_taskset ts(tasks, u_sum, u_max, pmin, pmax);
    failures += ts.simulate();
    std::cerr << ".";
    std::cerr.flush();
  }
  std::cerr << std::endl;
  return failures;
}

int main(int argc, char *argv[]) {
  using namespace std::chrono;
  namespace po = boost::program_options;

  po::options_description desc(
      "Generate and execute periodic task sets on atlas.");

  size_t tasks;
  size_t count;
  int64_t usum;
  int64_t umax;
  int64_t pmin;
  int64_t pmax;
  int64_t limit;

  // clang-format off
  desc.add_options()
    ("help", "Produce help message")
    ("tasks", po::value(&tasks)->default_value(2),
     "Number of tasks in the task set. (Default: 1)")
    ("count", po::value(&count)->default_value(1000),
     "Number of task sets to generate. (Default: 1000)")
    ("utilization", po::value(&usum)->default_value(1000),
     "Utilization of the task sets * 1e-3. (Default: 1000)")
    ("task-utilization", po::value(&umax)->default_value(500),
     "Maximum utilization of any task * 1e-3. (Default: 500)")
    ("exectime", "Find minimum execution time")
    ("min-period", po::value(&pmin)->default_value(10),
     "Minimum period of any task. (Default: 10ms)")
    ("max-period", po::value(&pmax)->default_value(100),
     "Maximum period of any task. (Default: 100ms)")
    ("no-preroll", "Disable preroll (Default: on)")
    ("duration",
     "Experiment duration for task sets with current parameters.")
    ("limit", po::value(&limit)->default_value(0),
     "Limit the hyperperiod to <num> seconds. (Default: 0, off)");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc;
    return EXIT_SUCCESS;
  }

  if (vm.count("exectime")) {
    find_minimum_e(count, period{pmin});
    return EXIT_SUCCESS;
  }

  if (vm.count("duration")) {
    ::duration(tasks, U{usum}, U{umax}, count, period{pmin}, period{pmax},
               s{limit});
    return EXIT_SUCCESS;
  }

  for (size_t task = 2; task <= tasks; ++task) {
    auto failures = schedulable(task, U{usum}, U{umax}, count, period{pmin},
                                period{pmax}, !vm.count("no-preroll"));
  }
}
