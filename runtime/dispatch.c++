#include <list>
#include <thread>
#include <condition_variable>
#include <iterator>
#include <vector>
#include <stdexcept>

#include <unistd.h>
#include <sys/syscall.h>
#include <csignal>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "predictor/predictor.h"
#include "atlas/atlas.h"

#include "dispatch.h"
#include "dispatch-internal.h"
#include "cputime_clock.h"

static thread_local atlas::dispatch_queue *current_queue;

static pid_t gettid() { return static_cast<pid_t>(syscall(SYS_gettid)); }

class Options {
  bool use_gcd_ = false;
  bool use_atlas_ = true;
  /* estimator dump */
public:
  Options() {
    /* ATLAS_BACKEND:
     *  - GCD use APPLE GCD
     *  - ATLAS use ATLAS (default when not set)
     *  - NONE use ATLAS queuing w/o kernel support
     */
    try {
      std::string backend(std::getenv("ATLAS_BACKEND"));
      if (backend == "GCD") {
        use_gcd_ = true;
      } else if (backend == "ATLAS") {
        use_atlas_ = true;
      } else if (backend == "NONE") {
        use_gcd_ = false;
        use_atlas_ = false;
      }
    } catch (...) {
    }
  }

  bool atlas() const { return use_atlas_; }
  bool gcd() const { return use_gcd_; }
};


namespace atlas {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
static Options options;
static atlas::estimator application_estimator;
#pragma clang diagnostic pop

namespace {
static auto cpu_set_to_vector(cpu_set_t *cpu_set) {
  std::vector<int> cpus;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
    if (CPU_ISSET(cpu, cpu_set))
      cpus.push_back(cpu);
  }
  return cpus;
}

static auto cpu_set_to_vector(size_t setsize, cpu_set_t *cpu_set) {
  std::vector<int> cpus;
  for (int cpu = 0; static_cast<size_t>(cpu) < setsize; ++cpu) {
    if (CPU_ISSET_S(cpu, setsize, cpu_set))
      cpus.push_back(cpu);
  }
  return cpus;
}

static void ignore_deadlines() {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
#if defined(__GNU_LIBRARY__) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
  act.sa_handler = SIG_IGN;
#if defined(__GNU_LIBRARY__) && defined(__clang__)
#pragma clang diagnostic pop
#endif
  if (sigaction(SIGXCPU, &act, nullptr)) {
    throw std::runtime_error("Error " + std::to_string(errno) + " " +
                             strerror(errno));
  }
}
}

static work_item *next_work_item() {
  uint64_t id;
  static_assert(sizeof(id) >= sizeof(work_item *),
                "id must have at least pointer size.");
  auto num = next(id);
  if (num > 1) {
    throw std::runtime_error("Returning more than 1 job is not implemented");
  } else if (num == 1) {
    return reinterpret_cast<work_item *>(static_cast<uintptr_t>(id));
  } else {
    return nullptr;
  }
}

class executor {
  mutable std::condition_variable empty;
  mutable std::mutex list_lock;
  mutable std::list<work_item> work_queue;

  virtual void submit(const uint64_t id,
                      const std::chrono::nanoseconds exectime,
                      const std::chrono::steady_clock::time_point deadline) const = 0;
  mutable std::atomic_bool done{false};

protected:
  void shutdown() const {
    using namespace std::literals::chrono_literals;
    enqueue({{},
             0us,
             nullptr,
             0,
             0,
             std::packaged_task<void()>([=] {
               done = true;
               empty.notify_all();
             }),
             false});
  }

  void work_loop() const {
    while (!done) {
      std::list<work_item> tmp;

      {
        std::unique_lock<std::mutex> lock(list_lock);
        empty.wait(lock, [this] { return (!work_queue.empty() || done); });

        if (done) {
          break;
        }

        /* do non-real-time work in order - pop from the front as long as there
         * is work */
        if (!work_queue.front().is_realtime) {
          tmp.splice(tmp.cbegin(), work_queue, work_queue.cbegin());
        } else {
          /* do real-time work */
          auto ptr = next_work_item();

          if (ptr != nullptr) {
            auto it =
                std::find_if(work_queue.cbegin(), work_queue.cend(),
                             [ptr](const auto &work) { return &work == ptr; });
            if (it == work_queue.cend()) {
              throw std::runtime_error("Work item not found.");
            }

            tmp.splice(tmp.cbegin(), work_queue, it);
          } else {
            /* no non-rt work and the rt work got someone else. go back to
             * sleep. */
            continue;
          }
        }
      }

      work_item &work = tmp.front();

      try {
        auto start = cputime_clock::now();
        // might throw std::future_error, if already invoked
        work.work();
        auto end = cputime_clock::now();

        if (work.is_realtime) {
          using namespace std::chrono;
          const auto exectime = end - start;
          const uint64_t id = reinterpret_cast<uint64_t>(&work);
          try {
            application_estimator.train(work.type, id,
                                        duration_cast<microseconds>(exectime));
          } catch (const std::runtime_error &e) {
            std::cerr << e.what() << std::endl;
            std::terminate();
          }
        }
      } catch (const std::runtime_error &e) {
        // Bad library, wrinting to cerr!
        std::cerr << e.what() << std::endl;
      }
    };
  }

public:
  virtual ~executor();
  void enqueue(work_item work) const {
    std::list<work_item> tmp;
    tmp.push_back(std::move(work));

    {
      auto &item = tmp.front();
      const uint64_t id = reinterpret_cast<uint64_t>(&item);
      if (item.is_realtime) {
        const auto exectime = application_estimator.predict(
            item.type, id, item.metrics, item.metrics_count);
        submit(id, exectime, item.deadline);
      }
    }

    {
      std::lock_guard<std::mutex> lock(list_lock);
      work_queue.splice(work_queue.end(), std::move(tmp));
    }

    empty.notify_all();
  }
};

executor::~executor() {
}

struct dispatch_queue::impl {
  uint32_t magic = 0x61746C73; // 'atls'
  std::string label_;
  std::unique_ptr<executor> worker;

  impl(dispatch_queue *queue, std::string label);
  impl(dispatch_queue *queue, std::string label, std::vector<int> cpu_set);
  void dispatch(work_item item) const { worker->enqueue(std::move(item)); }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
class queue_worker final : public executor {
#pragma clang diagnostic pop
  mutable std::thread thread;

  void process_work(dispatch_queue *queue) {
    ignore_deadlines();
    current_queue = queue;
    executor::work_loop();
  }

  void
  submit(const uint64_t id, const std::chrono::nanoseconds exectime,
         const std::chrono::steady_clock::time_point deadline) const override {
    np::submit(thread, id, exectime, deadline);
  }

public:
  queue_worker(dispatch_queue *queue)
      : thread(&queue_worker::process_work, this, queue) {}
  ~queue_worker() {
    shutdown();

    if (thread.joinable())
      thread.join();
  }
};

class pool {
  uint64_t id = static_cast<uint64_t>(-1);

public:
  pool() : id(atlas::threadpool::create()) {}
  ~pool() {
    if (id != static_cast<uint64_t>(-1))
      atlas::threadpool::destroy(id);
  }

  void join() const {
    if (id == static_cast<uint64_t>(-1))
      throw std::runtime_error("Invalid thread pool.");
    if (atlas::threadpool::join(id) < 0)
      throw std::runtime_error(strerror(errno));
  }

  template <class Rep, class Period, class Clock>
  void submit(const uint64_t jid,
              const std::chrono::duration<Rep, Period> exec_time,
              const std::chrono::time_point<Clock> deadline) const {
    atlas::threadpool::submit(id, jid, exec_time, deadline);
  }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
class concurrent final : public executor {
#pragma clang diagnostic pop
  pool tp;
  std::unique_ptr<std::thread[]> workers;
  size_t thread_count;

  std::atomic<size_t> init{0};

  void process_work(dispatch_queue *queue, int cpu) {
    {
      const auto tid = gettid();
      cpu_set_t cpu_set;
      CPU_ZERO(&cpu_set);
      CPU_SET(cpu, &cpu_set);

      sched_setaffinity(tid, sizeof(cpu_set_t), &cpu_set);
    }

    ignore_deadlines();
    tp.join();
    current_queue = queue;
    --init;

    executor::work_loop();
  }

  void
  submit(const uint64_t id, const std::chrono::nanoseconds exectime,
         const std::chrono::steady_clock::time_point deadline) const override {
    tp.submit(id, exectime, deadline);
  }

public:
  concurrent(dispatch_queue *queue, std::vector<int> cpu_set)
      : workers(std::make_unique<std::thread[]>(cpu_set.size())),
        thread_count(cpu_set.size()), init(thread_count) {
    for (size_t i = 0; i < cpu_set.size(); ++i) {
      workers[i] =
          std::thread(&concurrent::process_work, this, queue, cpu_set[i]);
    }

    /* wait until all threads are up, to avoid loosing a submit, when the thread
     * pool is still empty */
    for (; init;) {
      std::this_thread::yield();
    }
  }
  ~concurrent() {
    shutdown();

    for (size_t i = 0; i < thread_count; ++i) {
      if (workers[i].joinable()) {
        workers[i].join();
      }
    }
  }
};

dispatch_queue::impl::impl(dispatch_queue *queue, std::string label)
    : label_(std::move(label)),
#if defined(__clang__) && defined(__block)
      worker(options.gcd() ? make_gcd_queue(label_)
                           : std::make_unique<queue_worker>(queue))
#else
      worker(std::make_unique<queue_worker>(queue))
#endif
{
}

dispatch_queue::impl::impl(dispatch_queue *queue, std::string label,
                           std::vector<int> cpu_set)
    : label_(std::move(label)),
      worker(std::make_unique<concurrent>(queue, cpu_set)) {}

dispatch_queue::dispatch_queue(std::string label)
    : d_(std::make_unique<impl>(this, std::move(label))) {}

dispatch_queue::dispatch_queue(std::string label,
                               std::initializer_list<int> cpu_set)
    : d_(std::make_unique<impl>(this, std::move(label), cpu_set)) {}

dispatch_queue::dispatch_queue(std::string label, cpu_set_t *cpu_set)
    : d_(std::make_unique<impl>(this, std::move(label),
                                cpu_set_to_vector(cpu_set))) {}

dispatch_queue::~dispatch_queue() = default;

std::future<void> dispatch_queue::dispatch(std::function<void()> f) const {
  using namespace std::literals::chrono_literals;
  auto item = work_item{std::chrono::steady_clock::now(),
                        0us,
                        nullptr,
                        0,
                        static_cast<uint64_t>(-1),
                        std::packaged_task<void()>(f),
                        true};
  auto future = item.work.get_future();
  d_->dispatch(std::move(item));
  return future;
}

std::future<void>
dispatch_queue::dispatch(const std::chrono::steady_clock::time_point deadline,
                         const double *metrics, const size_t metrics_count,
                         const uint64_t type,
                         std::function<void()> block) const {
  using namespace std::literals::chrono_literals;
  auto item = work_item{deadline,       0us,  metrics,
                        metrics_count,  type, std::packaged_task<void()>(block),
                        options.atlas()};
  auto future = item.work.get_future();
  d_->dispatch(std::move(item));
  return future;
}
}
