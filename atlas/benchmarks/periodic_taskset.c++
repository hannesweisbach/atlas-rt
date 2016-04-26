#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <iomanip>

#include <cstdlib>
#include <cstring>
#include <cerrno>

#include <sched.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include <boost/math/common_factor_rt.hpp>
#include <boost/program_options.hpp>

#include "atlas/atlas.h"
#include "common/common.h"

#include "taskgen.h"

#define SCHED_DEADLINE	6

struct sched_attr {
  __u32 size;

  __u32 sched_policy;
  __u64 sched_flags;

  /* SCHED_NORMAL, SCHED_BATCH */
  __s32 sched_nice;

  /* SCHED_FIFO, SCHED_RR */
  __u32 sched_priority;

  /* SCHED_DEADLINE (nsec) */
  __u64 sched_runtime;
  __u64 sched_deadline;
  __u64 sched_period;
};

static decltype(auto) sched_setattr(pid_t pid, const struct sched_attr *attr,
                                    unsigned int flags) {
  return syscall(__NR_sched_setattr, pid, attr, flags);
}

static decltype(auto) sched_getattr(pid_t pid, struct sched_attr *attr,
                                    unsigned int size, unsigned int flags) {
  return syscall(__NR_sched_getattr, pid, attr, size, flags);
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

struct result {
  int64_t jobs{0};
  int64_t missed{0};

  result() = default;
  result(int64_t jobs_, int64_t missed_) : jobs(jobs_), missed(missed_) {}

  result operator+(const result &rhs) {
    return result{jobs + rhs.jobs, missed + rhs.missed};
  }

  result &operator+=(const result &rhs) {
    jobs += rhs.jobs;
    missed += rhs.missed;
    return *this;
  }

  friend std::ostream &operator<<(std::ostream &os, const result &rhs) {
    return os << rhs.missed << " " << rhs.jobs;
  }
};

class periodic_taskset {
  struct task {
    task_attr attr;
    pid_t tid;
    int64_t jobs;
    mutable std::atomic<uint64_t> deadline_misses{0};
    mutable std::mutex lock;
    mutable std::condition_variable cv;
    mutable uint64_t count = 0;
    mutable std::chrono::steady_clock::time_point deadline;
    bool edf;
    mutable std::atomic_bool init{false};
    mutable std::atomic_bool done{false};

    auto submit(uint64_t id, std::chrono::steady_clock::time_point d) const {
      auto dl = d + attr.p;
      if (edf) {
        std::lock_guard<std::mutex> l(lock);
        ++count;
        deadline = dl;
        cv.notify_one();
      } else {
        atlas::submit(tid, id, attr.e, dl);
      }
      return dl;
    }

    auto next_edf() const {
      using namespace std::chrono;
      steady_clock::time_point dl;

      {
        std::unique_lock<std::mutex> l(lock);
        cv.wait(l, [this] { return count; });
        --count;
        dl = deadline;
      }
//#define MEASURE_OVERHEAD
#ifndef MEASURE_OVERHEAD
      do_work(attr.e - 100us);
#else
      do_work(1ms);
#endif

      return std::chrono::steady_clock::now() > dl;
    }

    auto next_atlas() const {
      using namespace std::chrono;
      uint64_t id;
      atlas::next(id);

#ifndef MEASURE_OVERHEAD
      do_work(attr.e - 200us);
#else
      do_work(1ms);
#endif

      return reset_deadline();
    }

    auto next(const int64_t job) const {
      bool missed = false;

      if (edf) {
        missed = next_edf();
      } else {
        missed = next_atlas();
      }

      if (missed) {
        ++deadline_misses;
#if 0
        std::cerr << "Task " << tid << " " << attr << " failed on job " << job
                  << "/" << jobs << "." << std::endl;
#endif
      }
    }
  };

  std::vector<task> tasks;
  std::unique_ptr<std::thread[]> threads;

  hyperperiod_t hyperperiod;
  std::atomic_bool stop{false};

  auto run(const size_t i) {
    using namespace std::chrono;
    const task &task = tasks.at(i);

    if (task.edf) {
      struct sched_attr attr;

      attr.size = sizeof(attr);
      attr.sched_flags = 0;
      attr.sched_nice = 0;
      attr.sched_priority = 0;

      attr.sched_policy = SCHED_DEADLINE;
      attr.sched_runtime =
          static_cast<__u64>(duration_cast<nanoseconds>(task.attr.e).count());
      attr.sched_period = attr.sched_deadline =
          static_cast<__u64>(duration_cast<nanoseconds>(task.attr.p).count());

      auto ret = sched_setattr(0, &attr, 0);
      if (ret < 0) {
        // std::cerr << "Error setting scheduler (" << errno
        //          << "): " << strerror(errno) << std::endl;

        //exit(EXIT_FAILURE);
      }
    } else {
      record_deadline_misses();
    }

    task.init = true;

    for (int64_t job = 0; job < task.jobs && !stop; ++job) {
      task.next(job);
    }

#if 1
    if (task.edf) {
      struct sched_param param;
      param.sched_priority = sched_get_priority_min(SCHED_OTHER);
      if (sched_setscheduler(0, SCHED_OTHER, &param)) {
        std::cerr << "Error setting scheduler (" << errno
                  << "): " << strerror(errno) << std::endl;
      }
    }
#endif
    task.done = true;
  }

  void synchronize_end() {
    using namespace std::chrono;
    stop = true;
    for (size_t i = 0; i < tasks.size(); ++i) {
      if (!tasks.at(i).done && !tasks.at(i).edf) {
        atlas::np::submit(threads[i], 0, 1s, 2s);
      }
      if (threads[i].joinable()) {
        threads[i].join();
      }
    }
  }

public:
  periodic_taskset(const size_t n, U usum, U umax, period p_min, period p_max,
                   const bool edf = false)
      : tasks(n), threads(std::make_unique<std::thread[]>(n)) {
    const auto attr = generate_taskset(n, usum, umax, p_min, p_max);
    hyperperiod = ::hyperperiod(attr);
    for (size_t i = 0; i < n; ++i) {
      auto &task = tasks.at(i);
      task.attr = attr.at(i);
      task.edf = edf;
      task.jobs = hyperperiod / task.attr.p;
    }

    for (size_t i = 0; i < n; ++i) {
      threads[i] = std::thread(&periodic_taskset::run, this, i);
      tasks.at(i).tid = atlas::np::from(threads[i]);
      while (!tasks.at(i).init)
        ;
    }
  }

  auto result() {
    struct result r;
    for (const auto &task : tasks) {
      r.jobs += task.jobs;
      r.missed += task.deadline_misses;
    }

    return r;
  }

  void simulate() {
    using namespace std::chrono;
    struct release {
      steady_clock::time_point r;
      task * t;
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

    size_t jobs = 0;
    std::vector<release> releases(tasks.size());
    for (size_t i = 0; i < releases.size(); ++i) {
      auto &release = releases.at(i);
      auto &task = tasks.at(i);

      release.r = t0;
      release.t = &task;
      jobs += static_cast<size_t>(task.jobs);
    }

    for (size_t job = 0; job < jobs; ++job) {
      for (auto &&release : releases) {
        if (release.r <= steady_clock::now()) {
          release.r = release.t->submit(release.count, release.r);
          ++release.count;
        }
      }

      std::sort(std::begin(releases), std::end(releases));
      std::this_thread::sleep_until(releases.front().r);
    }

    synchronize_end();

    {
      /* This seems necessary, since forking from FIFO threads seems broken. */
      struct sched_param param;
      param.sched_priority = sched_get_priority_min(SCHED_OTHER);
      if (sched_setscheduler(0, SCHED_OTHER, &param)) {
        std::cerr << "Error setting scheduler (" << errno
                  << "): " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
      }
    }
  }

  friend std::ostream &operator<<(std::ostream &os,
                                  const periodic_taskset &ts) {
    for (const auto &task : ts.tasks) {
      os << task.attr << std::endl;
    }
    return os;
  }
};

static void find_minimum_e(const size_t count, const period p,
                           const bool edf = false) {
  using namespace std::chrono;
  std::vector<std::pair<U, size_t>> data;

  set_procfsparam(attribute::preroll, 0);

  for (auto u = U{100}; u < U{1000}; ++u) {
    std::cerr << std::setw(6) << u;
    std::cerr.flush();
    data.emplace_back(u, 0);
    for (size_t j = 0; j < count; ++j) {
      periodic_taskset ts(1, u, u, p, p, edf);
      ts.simulate();
      if (ts.result().missed) {
        ++data.back().second;
      }

      std::this_thread::sleep_for(1ms);
    }
    std::cerr << std::setw(5) << data.back().second << std::endl;
  }

  std::ofstream file("data");
  for (const auto &v : data) {
    file << std::setw(8) << v.first << " " << std::setw(8) << v.second
         << std::endl;
  }
}

static auto duration(const size_t tasks, const U u_sum, const U u_max,
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

  return duration;
}

static auto schedulable(const size_t tasks, const U u_sum, const U u_max,
                        const size_t count, const period pmin,
                        const period pmax, const bool edf = false) {
  using namespace std::chrono;
  result failures;

  for (size_t j = 0; j < count; ++j) {
    periodic_taskset ts(tasks, u_sum, u_max, pmin, pmax, edf);
    ts.simulate();
    failures += ts.result();
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

  std::vector<size_t> tasks;
  size_t count;
  int64_t usum;
  int64_t umax;
  int64_t pmin;
  int64_t pmax;
  int64_t limit;

  // clang-format off
  desc.add_options()
    ("help", "Produce help message")
    ("tasks", po::value(&tasks)->multitoken(),
     "Number of tasks in the task set. (Default: 1)")
    ("count", po::value(&count)->default_value(200),
     "Number of task sets to generate. (Default: 200)")
    ("utilization", po::value(&usum)->default_value(1000),
     "Utilization of the task sets * 1e-3. (Default: 1000)")
    ("task-utilization", po::value(&umax)->default_value(1000),
     "Maximum utilization of any task * 1e-3. (Default: 1000)")
    ("exectime", "Find minimum execution time")
    ("min-period", po::value(&pmin)->default_value(10),
     "Minimum period of any task. (Default: 10ms)")
    ("max-period", po::value(&pmax)->default_value(100),
     "Maximum period of any task. (Default: 100ms)")
    ("no-preroll", "Disable preroll (Default: on)")
    ("idle-pull", "Enable idle-pull (Default: off)")
    ("overload-push", "Enable overload-push (Default: off)")
    ("duration",
     "Experiment duration for task sets with current parameters.")
    ("limit", po::value(&limit)->default_value(0),
     "Limit the hyperperiod to <num> seconds. (Default: 0, off)")
    ("edf", "Use Linux' SCHED_DEADLINE.");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc;
    return EXIT_SUCCESS;
  }

  if (!vm.count("edf")) {
    set_procfsparam(attribute::job_stealing, vm.count("idle-pull"));
    set_procfsparam(attribute::overload_push, vm.count("overload-push"));
    set_procfsparam(attribute::preroll, !vm.count("no-preroll"));
  }

  if (tasks.empty())
    tasks.push_back(2);

  if (vm.count("exectime")) {
    find_minimum_e(count, period{pmin}, vm.count("edf"));
    return EXIT_SUCCESS;
  }

  if (vm.count("duration")) {
    hyperperiod_t duration{0};

    for (size_t task = tasks.front(); task <= tasks.back(); ++task) {
      duration += ::duration(task, U{usum}, U{umax}, count, period{pmin},
                             period{pmax}, s{limit});
    }

    std::cout << duration_cast<s>(duration).count() << std::endl;
    return EXIT_SUCCESS;
  }

  for (size_t task = tasks.front(); task <= tasks.back(); ++task) {
    if (static_cast<int64_t>(task) * umax < usum) {
      std::cout << "nan nan" << std::endl;
      continue;
    }
    auto failures = schedulable(task, U{usum}, U{umax}, count, period{pmin},
                                period{pmax}, vm.count("edf"));
    std::cout << failures << std::endl;
    std::cerr << failures << " deadline misses with " << task << " tasks "
              << std::endl;
  }
}
