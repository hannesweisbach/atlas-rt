#include <iostream>

#include <dlfcn.h>

#include <dispatch/dispatch.h>

#include "dispatch-internal.h"

namespace atlas {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
static std::ostream &operator<<(std::ostream &os, const Dl_info &info) {
#pragma clang diagnostic pop
  return os << info.dli_fbase << " from " << info.dli_fname;
}

struct libdispatch {
  using dispatch_block_t = void (^)(void);
  using dispatch_function_t = void (*)(void);
  using object_t = void (*)(dispatch_object_t object);
  using queue_create_t = dispatch_queue_t (*)(const char *label,
                                              dispatch_queue_attr_t attr);
  using dispatch_t = void (*)(dispatch_queue_t queue, dispatch_block_t);
  using dispatch_f_t = void (*)(dispatch_queue_t queue, void *context,
                                dispatch_function_t);

  void *handle = nullptr;

  object_t dispatch_release_;
  queue_create_t dispatch_queue_create_;
  dispatch_t dispatch_async_;
  dispatch_f_t dispatch_async_f_;
  dispatch_t dispatch_barrier_sync_;

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
    dispatch_queue_create_ = reinterpret_cast<queue_create_t>(
        dlsym(handle, "dispatch_queue_create"));
    dispatch_release_ =
        reinterpret_cast<object_t>(dlsym(handle, "dispatch_release"));
    dispatch_async_ =
        reinterpret_cast<dispatch_t>(dlsym(handle, "dispatch_async"));
    dispatch_async_f_ =
        reinterpret_cast<dispatch_f_t>(dlsym(handle, "dispatch_async_f"));
    dispatch_barrier_sync_ =
        reinterpret_cast<dispatch_t>(dlsym(handle, "dispatch_barrier_sync"));
  }

  ~libdispatch() {
    if (handle) {
      dlclose(handle);
    }
  }

  auto dispatch_queue_create(const char *label,
                             dispatch_queue_attr_t attr) const {
    return dispatch_queue_create_(label, attr);
  }

  auto dispatch_release(dispatch_object_t object) const {
    return dispatch_release_(object);
  }

  auto dispatch_async(dispatch_queue_t queue, dispatch_block_t block) const {
    return dispatch_async_(queue, block);
  }

  void dispatch_barrier_sync(dispatch_queue_t queue,
                             dispatch_block_t block) const {
    dispatch_barrier_sync_(queue, block);
  }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
static libdispatch gcd;
#pragma clang diagnostic pop

class gcd_worker final : public executor {
  dispatch_queue_t gcd_queue;

public:
  gcd_worker(const std::string &);
  ~gcd_worker();

  void enqueue(work_item work) const override;
};

gcd_worker::gcd_worker(const std::string &label)
    : gcd_queue(gcd.dispatch_queue_create(("com.atlas.worker." + label).c_str(),
                                          nullptr)) {}
gcd_worker::~gcd_worker() {
  gcd.dispatch_barrier_sync(gcd_queue, ^{
                            });
  gcd.dispatch_release(gcd_queue);
}

void gcd_worker::enqueue(work_item work) const {
  /* This is a debug aid only, so let's be disgustingly inefficient. */
  auto *task = new std::packaged_task<void()>(std::move(work.work));
  gcd.dispatch_async(gcd_queue, ^{
    (*task)();
    delete task;
  });
}

std::unique_ptr<executor> make_gcd_queue(const std::string &label) {
  return std::make_unique<gcd_worker>(label);
}
}
