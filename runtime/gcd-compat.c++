#include <chrono>

#include <dlfcn.h>
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

dispatch_queue_attr_t DISPATCH_QUEUE_SERIAL = NULL;

dispatch_queue_t dispatch_queue_create(const char *label,
                                       dispatch_queue_attr_t) {
  return reinterpret_cast<dispatch_queue_t>(
      new atlas::dispatch_queue(std::string(label)));
}

void dispatch_queue_release(dispatch_queue_t queue) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  delete queue_;
}

void dispatch_release(dispatch_queue_t queue) { dispatch_queue_release(queue); }

#ifdef __BLOCKS__
void dispatch_async(dispatch_queue_t queue, dispatch_block_t block) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_async(block);
}

void dispatch_sync(dispatch_queue_t queue, dispatch_block_t block) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_sync(block);
}
#endif

void dispatch_async_f(dispatch_queue_t queue, void *context,
                      dispatch_function_t function) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_async(function, context);
}

void dispatch_sync_f(dispatch_queue_t queue, void *context,
                     dispatch_function_t function) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_sync(function, context);
}

#ifdef __BLOCKS__
void dispatch_async_atlas(dispatch_queue_t queue,
                          const struct timespec *deadline,
                          const double *metrics, const size_t metrics_count,
                          dispatch_block_t block) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_async_atlas(to_time_point(deadline), metrics, metrics_count,
                               block);
}

void dispatch_sync_atlas(dispatch_queue_t queue,
                         const struct timespec *deadline, const double *metrics,
                         const size_t metrics_count, dispatch_block_t block) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_sync_atlas(to_time_point(deadline), metrics, metrics_count,
                              block);
}
#endif

void dispatch_async_atlas_f(dispatch_queue_t queue,
                            const struct timespec *deadline,
                            const double *metrics, const size_t metrics_count,
                            void *context, dispatch_function_t function) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_async_atlas(to_time_point(deadline), metrics, metrics_count,
                               function, context);
}

void dispatch_sync_atlas_f(dispatch_queue_t queue,
                           const struct timespec *deadline,
                           const double *metrics, const size_t metrics_count,
                           void *context, dispatch_function_t function) {
  auto queue_ = reinterpret_cast<atlas::dispatch_queue *>(queue);
  queue_->dispatch_sync_atlas(to_time_point(deadline), metrics, metrics_count,
                              function, context);
}

struct libdispatch {
#ifdef __BLOCKS__
  void (*dispatch_once)(dispatch_once_t *predicate, dispatch_block_t block);
#endif
  void (*dispatch_once_f)(dispatch_once_t *predicate, void *context,
                          dispatch_function_t function);

  void *handle = nullptr;


  /* RTLD_DEEPBIND (since glibc 2.3.4)
   *   Place the lookup scope of the symbols in this library ahead of the
   *   global scope.  This means that a self-contained library will use its own
   *   symbols in preference to global symbols with the same name contained in
   *   libraries that have already been loaded.  This flag is not specified in
   *   POSIX.1-2001.
   * Alternatively, one could use -Bsymbolic[-functions] when linking.
   */
  libdispatch() : handle(dlopen("libdispatch.so", RTLD_LAZY | RTLD_DEEPBIND)) {
    if (handle == nullptr)
      throw std::runtime_error("Unable to load libdispatch.so");
#ifdef __BLOCKS__
    dispatch_once = reinterpret_cast<decltype(dispatch_once)>(
        dlsym(handle, "dispatch_once"));
#endif
    dispatch_once_f = reinterpret_cast<decltype(dispatch_once_f)>(
        dlsym(handle, "dispatch_once_f"));
  }

  ~libdispatch() {
    if (handle) {
      dlclose(handle);
    }
  }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
static libdispatch gcd;
#pragma clang diagnostic pop

#ifdef __BLOCKS__
void dispatch_once(dispatch_once_t *predicate, dispatch_block_t block) {
  gcd.dispatch_once(predicate, block);
}
#endif

void dispatch_once_f(dispatch_once_t *predicate, void *context,
                     dispatch_function_t function) {
  gcd.dispatch_once_f(predicate, context, function);
}

struct timespec atlas_now(void) {
  using namespace std::chrono;
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  struct timespec ts;
  ts.tv_sec = duration_cast<seconds>(now).count();
  ts.tv_nsec = duration_cast<nanoseconds>(now - seconds(ts.tv_sec)).count();
  return ts;
}

dispatch_queue_t dispatch_get_main_queue() {
  return reinterpret_cast<dispatch_queue_t>(
      &atlas::dispatch_queue::dispatch_get_main_queue());
}
void dispatch_main(void) { atlas::dispatch_queue::dispatch_main(); }
}

