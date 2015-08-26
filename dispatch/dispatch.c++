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

#include "predictor/predictor.h"
#include "atlas/atlas.h"

#include "dispatch.h"
#include "cputime_clock.h"

static thread_local atlas::dispatch_queue *current_queue;

static pid_t gettid() { return static_cast<pid_t>(syscall(SYS_gettid)); }

namespace atlas {

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
  if(sigaction(SIGXCPU, &act, nullptr)) {
    throw std::runtime_error("Error " + std::to_string(errno) + " " +
                             strerror(errno));
  }
}
}

struct dispatch_queue::impl {
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


  class queue_worker {
    mutable std::condition_variable empty;
    mutable std::mutex list_lock;
    mutable std::list<work_item> work_queue;
    atlas::estimator *estimator;
    std::thread thread;

    bool done = false;
    void process_work(dispatch_queue *queue, std::promise<pid_t> promise) {
      ignore_deadlines();
      current_queue = queue;
      promise.set_value(gettid());

      while (!done) {
        std::list<work_item> tmp;
        {
          std::unique_lock<std::mutex> lock(list_lock);
          empty.wait(lock, [=] { return !work_queue.empty(); });
          tmp.splice(tmp.cbegin(), work_queue, work_queue.cbegin());
        }

        work_item &work = tmp.front();

        if (work.is_realtime)
          next();

        auto start = cputime_clock::now();

        try {
          work.work();
          work.completion->set_value();
        } catch (...) {
          work.completion->set_exception(std::current_exception());
        }

        auto end = cputime_clock::now();

        //TODO do not train, if exception occured. Remove instead.
        if (work.is_realtime) {
          using namespace std::chrono;
          const auto exectime = end - start;
          const uint64_t id = reinterpret_cast<uint64_t>(&work);
          estimator->train(work.type, id,
                           duration_cast<microseconds>(exectime));
        }
      };
    }

  public:
    queue_worker(dispatch_queue *queue, atlas::estimator *estimator_,
                 std::promise<pid_t> promise)
        : estimator(estimator_),
          thread(&queue_worker::process_work, this, queue, std::move(promise)) {
    }
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

    void enqueue(work_item work) const {
      std::list<work_item> tmp;
      tmp.push_back(std::move(work));

      {
        auto &item = tmp.front();
        const uint64_t id = reinterpret_cast<uint64_t>(&item);
        if (item.is_realtime) {
          const auto exectime = estimator->predict(item.type, id, item.metrics,
                                                   item.metrics_count);
          np::submit(thread, id, exectime, work.deadline);
        } else {
          using namespace std::literals::chrono_literals;
          np::submit(thread, id, 0s, work.deadline);
        }
      }

      {
        std::lock_guard<std::mutex> lock(list_lock);
        work_queue.splice(work_queue.end(), tmp);
      }

      empty.notify_one();
    }
  };

  struct worker {
    pid_t tid;
    std::unique_ptr<std::thread> t;
  };
  uint32_t magic = 0x61746C73; // 'atls'
  std::string label_;
  std::unique_ptr<estimator> estimator_ctx;
  std::unique_ptr<queue_worker> worker;

  impl(dispatch_queue *queue, std::string label)
      : label_(std::move(label)), estimator_ctx(std::make_unique<estimator>()) {
    std::promise<pid_t> promise;
    auto fut = promise.get_future();
    worker = std::make_unique<queue_worker>(queue, estimator_ctx.get(),
                                            std::move(promise));
    fut.get();
  }
  impl(dispatch_queue *queue, std::string label, std::vector<int> cpu_set)
      : label_(std::move(label)), estimator_ctx(std::make_unique<estimator>()) {
    std::promise<pid_t> promise;
    auto fut = promise.get_future();
    worker = std::make_unique<queue_worker>(queue, estimator_ctx.get(),
                                            std::move(promise));
    fut.get();
  }
  void dispatch(work_item item) const { worker->enqueue(std::move(item)); }
};

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
