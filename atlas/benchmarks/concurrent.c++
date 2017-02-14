#include <algorithm>
#include <atomic>
#include <memory>
#include <numeric>
#include <vector>
#include <iomanip>
#include <chrono>

#include <boost/math/common_factor_rt.hpp>
#include <boost/program_options.hpp>

#include "atlas/atlas.h"
#include "utils/common.h"

#include "taskgen.h"
#include "common.h"

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

using namespace std::chrono;

class concurrent_queue {
  uint64_t tp;
  size_t num_threads;
  std::vector<task_attr> tasks;
  std::unique_ptr<std::thread[]> threads;

  hyperperiod_t hyperperiod;
  int64_t jobs_;
  std::atomic<int64_t> jobs;
  std::atomic<int64_t> deadline_misses{0};
  std::atomic<uint64_t> init;
  std::atomic<uint64_t> done;

  struct task_params {
    nanoseconds e;
    steady_clock::time_point dl;
  };
  std::vector<task_params> params;

  void work(const uint64_t tp_, std::atomic<int64_t> &jobs_,
            std::atomic<int64_t> &deadline_misses_,
            std::atomic<uint64_t> &init_, std::atomic<uint64_t> &done_,
            size_t cpu) {
    set_affinity(static_cast<unsigned>(cpu));
    atlas::threadpool::join(tp_);
    record_deadline_misses();

    --init_;

    for (;;) {
      using namespace std::chrono;
      uint64_t id = 0;
      while (jobs_ > 0 && atlas::next(id) != 1)
        ;

      if (jobs_ <= 0) {
        break;
      }

      --jobs;
//#define MEASURE_OVERHEAD
#ifdef MEASURE_OVERHEAD
      do_work(1ms);
#else
      const auto params = reinterpret_cast<task_params *>(id);
      do_work(params->e - 300us);
#endif
      if (steady_clock::now() > params->dl)
        ++deadline_misses_;
    }

    --done_;
  }

  void synchronize_end() {
    using namespace std::chrono;
    for (size_t i = 0; i < num_threads; ++i) {
      for (; done;) {
        std::this_thread::sleep_for(10ms);
      }
      if (threads[i].joinable()) {
        threads[i].join();
      }
    }
  }

public:
  concurrent_queue(const size_t n, U usum, U umax, period p_min, period p_max,
                   const bool exectime = false)
      : tp(atlas::threadpool::create()),
        num_threads(exectime ? 1 : std::thread::hardware_concurrency()),
        tasks(generate_taskset(n, usum, umax, p_min, p_max)),
        threads(std::make_unique<std::thread[]>(num_threads)),
        hyperperiod(::hyperperiod(tasks)),
        jobs_(std::accumulate(std::begin(tasks), std::end(tasks), int64_t(0),
                              [this](const auto &sum, const auto &rhs) {
                                return sum + (hyperperiod / rhs.p);
                              })),

        jobs(jobs_), params(jobs), init(num_threads), done(num_threads) {
    using namespace std::chrono;


    for (size_t i = 0; i < num_threads; ++i) {
      threads[i] = std::thread(&concurrent_queue::work, this, tp,
                               std::ref(jobs), std::ref(deadline_misses),
                               std::ref(init), std::ref(done), i);
    }

    for (; init;)
      std::this_thread::sleep_for(1ms);
  }

  ~concurrent_queue() { atlas::threadpool::destroy(tp); }

  auto result() {
    struct result r;
    r.jobs = jobs_;
    r.missed = deadline_misses;
    return r;
  }

  void simulate() {
    using namespace std::chrono;
    struct release {
      steady_clock::time_point r;
      task_attr * t;
      size_t count;

      bool operator<(const release &rhs) { return r < rhs.r; }
    };

#if 0
    std::cout << "Running simulation for "
              << duration_cast<ms>(hyperperiod).count() << "s" << std::endl;
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
      release.t = &task;
    }

    int64_t job = 0;
    for (; job < jobs_;) {
      for (auto &&release : releases) {
        if (release.r <= steady_clock::now()) {
          auto dl = release.r + release.t->p;
          auto &&p = params.at(job);
          p.e = release.t->e;
          p.dl = dl;
          atlas::threadpool::submit(tp, reinterpret_cast<uint64_t>(&p),
                                    release.t->e, dl);
          release.r = dl;
          ++release.count;
          ++job;
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
};

static void find_minimum_e(const size_t count, const period p) {
  using namespace std::chrono;
  std::vector<std::pair<U, size_t>> data;

  for (auto u = U{100}; u < U{1000}; ++u) {
    std::cerr << std::setw(6) << u;
    std::cerr.flush();
    data.emplace_back(u, 0);
    for (size_t j = 0; j < count; ++j) {
      concurrent_queue ts(1, u, u, p, p);
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

static auto schedulable(const size_t tasks, const U u_sum, const U u_max,
                        const size_t count, const period pmin,
                        const period pmax) {
  using namespace std::chrono;
  result failures;

  for (size_t j = 0; j < count; ++j) {
    concurrent_queue ts(tasks, u_sum, u_max, pmin, pmax);
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
  std::vector<int64_t> usums;
  size_t count;
  int64_t umax;
  int64_t pmin;
  int64_t pmax;

  // clang-format off
  desc.add_options()
    ("help", "Produce help message")
    ("tasks", po::value(&tasks)->multitoken(),
     "Number of tasks in the task set. (Default: 1)")
    ("count", po::value(&count)->default_value(200),
     "Number of task sets to generate. (Default: 200)")
    ("utilization", po::value(&usums)->multitoken(),
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
    ("overload-push", "Enable overload-push (Default: off)");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc;
    return EXIT_SUCCESS;
  }

  set_procfsparam(attribute::job_stealing, vm.count("idle-pull"));
  set_procfsparam(attribute::overload_push, vm.count("overload-push"));
  set_procfsparam(attribute::preroll, !vm.count("no-preroll"));

  if (tasks.empty())
    tasks.push_back(2);
  if (usums.empty())
    usums.push_back(1000);

  std::cerr << gettid() << std::endl;
  //permute();

  if (vm.count("exectime")) {
    find_minimum_e(count, period{pmin});
    return EXIT_SUCCESS;
  }

  for (size_t task = tasks.front(); task <= tasks.back(); ++task) {
    for (const auto &usum : usums) {
      std::cerr << usum << " " << umax << std::endl;
      if (static_cast<int64_t>(task) * umax < usum) {
        std::cout << "nan nan" << std::endl;
        continue;
      }
      auto failures = schedulable(task, U{usum}, U{umax}, count, period{pmin},
                                  period{pmax});
      std::cout << double(usum) / 1000.0 << " " << task << " " << failures << std::endl;
      std::cerr << failures << " deadline misses with " << task << " tasks "
                << std::endl;
    }
  }

}

