#pragma once

#ifdef __cplusplus
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <memory>
#include <typeindex>
#include <initializer_list>
#include <ctime>

#include <iostream>

#else
#include <time.h>
#endif

#include <sched.h>

//#include <dispatch/dispatch.h>

#ifdef __cplusplus
namespace atlas {

namespace {

template <typename T> uint64_t work_type(T) {
  const auto type = std::type_index(typeid(T)).hash_code();
  //std::cout << "Function Ptr " << std::hex << type << std::endl;
  return type;
}

template <typename Ret, typename... Args>
uint64_t work_type(Ret (*&f)(Args...)) {
#if 0
  const auto type = reinterpret_cast<uint64_t>(f);
  std::cout << "Function Ptr " << std::hex << type << std::endl;
#endif
  return reinterpret_cast<uint64_t>(f);
}

template <typename Ret, typename... Args> uint64_t work_type(Ret(&f)(Args...)) {
#if 0
  const auto type = reinterpret_cast<uint64_t>(&f);
  std::cout << "Function Ref " << std::hex << type << std::endl;
#endif
  return reinterpret_cast<uint64_t>(&f);
}

#if defined(__clang__) && defined(__block)
template <typename Ret, typename... Args>
uint64_t work_type(Ret (^&f)(Args...)) {
  struct BlockLayout {
    void *isa;
    int flags;
    int reserved;
    void (*invoke)(void *, ...);
  };
  const auto type =
      reinterpret_cast<uint64_t>(reinterpret_cast<BlockLayout *>(f)->invoke);
  std::cout << "Block Ref " << std::hex << type << std::endl;
  return type;
}
#if 0
template <typename Ret, typename... Args>
uint64_t work_type(Ret (^*f)(Args...)) {
  const auto type = reinterpret_cast<uint64_t>(&f);
  std::cout << "Block Ptr " << std::hex << type << std::endl;
  return reinterpret_cast<uint64_t>(&f);
}
#endif
#endif
}

class dispatch_queue {
  struct impl;
  std::unique_ptr<impl> d_;

  std::future<void> dispatch(const std::chrono::steady_clock::time_point,
                             const double *, const size_t, const uint64_t,
                             std::function<void()>) const;
  std::future<void> dispatch(std::function<void()>) const;

public:
  struct attr {
    size_t num_threads;
  };

  /* serial queue */
  dispatch_queue(std::string label);
  /* parallel queue */
  dispatch_queue(std::string label, std::initializer_list<int> cpu_set);
  dispatch_queue(std::string label, cpu_set_t *cpu_set);
  ~dispatch_queue();

  template <typename Func, typename... Args>
  decltype(auto)
  dispatch_async_atlas(const std::chrono::steady_clock::time_point deadline,
                       const double *metrics, const size_t metrics_count,
                       Func &&block, Args &&... args) {
    const uint64_t type = work_type(block);
    return dispatch(
        deadline, metrics, metrics_count, type,
        std::bind(std::forward<Func>(block), std::forward<Args>(args)...));
  }

  template <typename Func, typename... Args>
  auto dispatch_sync_atlas(const std::chrono::steady_clock::time_point deadline,
                           const double *metrics, const size_t metrics_count,
                           Func &&block, Args &&... args) {
    return dispatch_async_atlas(deadline, metrics, metrics_count,
                                std::forward<Func>(block),
                                std::forward<Args>(args)...)
        .get();
  }

  template <typename Func, typename... Args>
  decltype(auto) dispatch_async(Func &&f, Args &&... args) {
    return dispatch(
        std::bind(std::forward<Func>(f), std::forward<Args>(args)...));
  }

  template <typename Func, typename... Args>
  auto dispatch_sync(Func &&f, Args &&... args) {
    return dispatch(std::bind(std::forward<Func>(f), std::forward<Args>(args)...))
        .get();
  }
};
}
#endif

