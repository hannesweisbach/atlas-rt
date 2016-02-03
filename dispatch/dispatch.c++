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
#include <cstring>

#include "predictor/predictor.h"
#include "atlas/atlas.h"

#include "dispatch.h"
#include "cputime_clock.h"

static thread_local atlas::dispatch_queue *current_queue;

static pid_t gettid() { return static_cast<pid_t>(syscall(SYS_gettid)); }

namespace atlas {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
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

struct work_item {
  std::chrono::steady_clock::time_point deadline;
  std::chrono::microseconds prediction;
  const double *metrics;
  size_t metrics_count;
  uint64_t type;
  std::function<void()> work;
  std::shared_ptr<std::promise<void>> completion;
  bool is_realtime;
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
class executor {
#pragma clang diagnostic pop
public:
  virtual ~executor() = default;
  virtual void enqueue(work_item) const = 0;
};

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
  mutable std::condition_variable empty;
  mutable std::mutex list_lock;
  mutable std::list<work_item> work_queue;
  std::thread thread;

  bool done = false;

  void process_work(dispatch_queue *queue) {
    ignore_deadlines();
    current_queue = queue;

    while (!done) {
      std::list<work_item> tmp;
      {
        std::unique_lock<std::mutex> lock(list_lock);
        empty.wait(lock, [=] { return !work_queue.empty(); });
        if (!tmp.front().is_realtime) {
          tmp.splice(tmp.cbegin(), work_queue, work_queue.cbegin());
        }
      }

      if (tmp.empty()) {
        uint64_t id;
        work_item *ptr;
        next(id);
        static_assert(sizeof(id) == sizeof(ptr),
                      "Types must be of equal size.");
        memcpy(&ptr, &id, sizeof(id));
        // TODO: lock queue.
        auto it =
            std::find_if(work_queue.cbegin(), work_queue.cend(),
                         [ptr](const auto &work) { return &work == ptr; });
        if (it == work_queue.cend()) {
          throw std::runtime_error("Work item not found.");
        }

        tmp.splice(tmp.cbegin(), work_queue, it);
      }

      work_item &work = tmp.front();

      try {
        auto start = cputime_clock::now();
        work.work();
        auto end = cputime_clock::now();

        work.completion->set_value();

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
      } catch (...) {
        work.completion->set_exception(std::current_exception());
      }
    };
  }

public:
  queue_worker(dispatch_queue *queue)
      : thread(&queue_worker::process_work, this, queue) {}
  ~queue_worker() {
    using namespace std::literals::chrono_literals;
    enqueue({{},
             0us,
             nullptr,
             0,
             0,
             [=] { done = true; },
             std::make_shared<std::promise<void>>(),
             false});
    if (thread.joinable())
      thread.join();
  }

  void enqueue(work_item work) const override {
    std::list<work_item> tmp;
    tmp.push_back(std::move(work));

    {
      auto &item = tmp.front();
      const uint64_t id = reinterpret_cast<uint64_t>(&item);
      if (item.is_realtime) {
        const auto exectime = application_estimator.predict(
            item.type, id, item.metrics, item.metrics_count);
        np::submit(thread, id, exectime, work.deadline);
      } else {
        // using namespace std::literals::chrono_literals;
        // np::submit(thread, id, 0s, work.deadline);
      }
    }

    {
      std::lock_guard<std::mutex> lock(list_lock);
      work_queue.splice(work_queue.end(), tmp);
    }

    empty.notify_one();
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
  mutable std::condition_variable empty;
  mutable std::mutex list_lock;
  mutable std::list<work_item> work_queue;
  mutable uint64_t count = 0;
  std::unique_ptr<std::thread[]> workers;
  size_t thread_count;

  std::atomic_bool done{false};

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

    while (!done) {
      std::list<work_item> tmp;
      {
        std::unique_lock<std::mutex> lock(list_lock);
        empty.wait(lock, [=] { return ((count != 0) || done); });
        if (done) {
          break;
        }
        --count;
        if (!tmp.front().is_realtime) {
          tmp.splice(tmp.cbegin(), work_queue, work_queue.cbegin());
        }
      }

      if (tmp.empty()) {
        uint64_t id;
        work_item *ptr;
        next(id);
        static_assert(sizeof(id) == sizeof(ptr),
                      "Types must be of equal size.");
        memcpy(&ptr, &id, sizeof(id));
        auto it =
            std::find_if(work_queue.cbegin(), work_queue.cend(),
                         [ptr](const auto &work) { return &work == ptr; });
        if (it == work_queue.cend()) {
          throw std::runtime_error("Work item not found.");
        }

        tmp.splice(tmp.cbegin(), work_queue, it);
      }

      work_item &work = tmp.front();

      try {
        auto start = cputime_clock::now();
        work.work();
        auto end = cputime_clock::now();

        work.completion->set_value();

        if (work.is_realtime) {
          using namespace std::chrono;
          const auto exectime = end - start;
          const uint64_t id = reinterpret_cast<uint64_t>(&work);
          application_estimator.train(work.type, id,
                                      duration_cast<microseconds>(exectime));
        }
      } catch (...) {
        work.completion->set_exception(std::current_exception());
      }
    };
  }

public:
  concurrent(dispatch_queue *queue, std::vector<int> cpu_set)
      : workers(std::make_unique<std::thread[]>(cpu_set.size())),
        thread_count(cpu_set.size()) {
    for (size_t i = 0; i < cpu_set.size(); ++i) {
      workers[i] =
          std::thread(&concurrent::process_work, this, queue, cpu_set[i]);
    }
  }
  ~concurrent() {
    using namespace std::literals::chrono_literals;
    enqueue({{},
             0us,
             nullptr,
             0,
             0,
             [=] {
               done = true;
               empty.notify_all();
             },
             std::make_shared<std::promise<void>>(),
             false});

    for (size_t i = 0; i < thread_count; ++i) {
      if (workers[i].joinable()) {
        workers[i].join();
      }
    }
  }

  void enqueue(work_item work) const override {
    std::list<work_item> tmp;
    tmp.push_back(std::move(work));

    {
      auto &item = tmp.front();
      const uint64_t id = reinterpret_cast<uint64_t>(&item);
      if (item.is_realtime) {
        const auto exectime = application_estimator.predict(
            item.type, id, item.metrics, item.metrics_count);
        tp.submit(id, exectime, work.deadline);
      } else {
        // using namespace std::literals::chrono_literals;
        // np::submit(thread, id, 0s, work.deadline);
      }
    }

    {
      std::lock_guard<std::mutex> lock(list_lock);
      work_queue.splice(work_queue.end(), tmp);
      ++count;
    }

    empty.notify_one();
  }
};


dispatch_queue::impl::impl(dispatch_queue *queue, std::string label)
    : label_(std::move(label)), worker(std::make_unique<queue_worker>(queue)) {}

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
  auto promise = std::make_shared<std::promise<void>>();
  auto future = promise->get_future();
  d_->dispatch({std::chrono::steady_clock::now(), 0us, nullptr, 0,
                static_cast<uint64_t>(-1), f, promise, false});
  return future;
}

std::future<void>
dispatch_queue::dispatch(const std::chrono::steady_clock::time_point deadline,
                         const double *metrics, const size_t metrics_count,
                         const uint64_t type,
                         std::function<void()> block) const {
  using namespace std::literals::chrono_literals;
  auto promise = std::make_shared<std::promise<void>>();
  auto future = promise->get_future();
  d_->dispatch(
      {deadline, 0us, metrics, metrics_count, type, block, promise, true});
  return future;
}
}

namespace {
static auto to_time_point(const struct timespec *const ts) {
  using namespace std::chrono;
  const auto s = seconds{ts->tv_sec};
  const auto ns = nanoseconds{ts->tv_nsec};
  return steady_clock::time_point{s + ns};
}
}

extern "C" {
dispatch_queue_t dispatch_queue_create(const char *label,
                                       dispatch_queue_attr_t attr) {
  return reinterpret_cast<dispatch_queue_t>(
      new atlas::dispatch_queue(std::string(label)));
}

void dispatch_queue_release(dispatch_queue_t queue) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  delete queue_;
}

#if defined(__clang__) && defined(__block)
void dispatch_async(dispatch_queue_t queue, void (^block)(void)) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_async(block);
}

void dispatch_sync(dispatch_queue_t queue, void (^block)(void)) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_sync(block);
}
#endif

void dispatch_async_f(dispatch_queue_t queue, void *context,
                      void (*function)(void *)) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_async(function, context);
}

void dispatch_sync_f(dispatch_queue_t queue, void *context,
                     void (*function)(void *)) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_sync(function, context);
}

#if defined(__clang__) && defined(__block)
void dispatch_async_atlas(dispatch_queue_t queue,
                          const struct timespec *deadline,
                          const double *metrics, const size_t metrics_count,
                          void (^block)(void));

void dispatch_sync_atlas(dispatch_queue_t queue,
                         const struct timespec *deadline, const double *metrics,
                         const size_t metrics_count, void (^block)(void)) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_sync_atlas(to_time_point(deadline), metrics, metrics_count,
                              block);
}
#endif

void dispatch_async_atlas_f(dispatch_queue_t queue,
                            const struct timespec *deadline,
                            const double *metrics, const size_t metrics_count,
                            void *context, void (*function)(void *)) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_async_atlas(to_time_point(deadline), metrics, metrics_count,
                               function, context);
}

void dispatch_sync_atlas_f(dispatch_queue_t queue,
                           const struct timespec *deadline,
                           const double *metrics, const size_t metrics_count,
                           void *context, void (*function)(void *)) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_sync_atlas(to_time_point(deadline), metrics, metrics_count,
                              function, context);
}
}
