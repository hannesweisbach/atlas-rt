#include <chrono>

#include "gcd-compat.h"
#include "dispatch.h"

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
                                       dispatch_queue_attr_t) {
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

