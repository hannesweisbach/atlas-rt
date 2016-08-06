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
#include <tuple>
#include <experimental/tuple>

#include <iostream>

#else
#include <time.h>
#endif

#include <sched.h>

#ifdef __BLOCKS__
#include <Block.h>
#endif

#ifdef __cplusplus
namespace atlas {

namespace {

template <typename T> uint64_t work_type(const T &) {
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

#ifdef __BLOCKS__
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
  dispatch_queue(dispatch_queue &&);
  dispatch_queue &operator=(dispatch_queue &&);
  ~dispatch_queue();

  template <typename Func, typename... Args>
  decltype(auto)
  dispatch_async_atlas(const std::chrono::steady_clock::time_point deadline,
                       const double *metrics, const size_t metrics_count,
                       Func &&block, Args &&... args) {
    const uint64_t type = work_type(block);
    return dispatch(deadline, metrics, metrics_count, type, [
      f_ = std::forward<Func>(block),
      args_ = std::make_tuple(std::forward<Args>(args)...)
    ]() mutable { std::experimental::apply(std::move(f_), std::move(args_)); });
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
    return dispatch([
      f_ = std::forward<Func>(f),
      args_ = std::make_tuple(std::forward<Args>(args)...)
    ]() mutable { std::experimental::apply(std::move(f_), std::move(args_)); });
  }

  template <typename Func, typename... Args>
  auto dispatch_sync(Func &&f, Args &&... args) {
    return dispatch_async(std::forward<Func>(f), std::forward<Args>(args)...)
        .get();
  }

#ifdef __BLOCKS__
  template <typename Ret, typename... Args>
  decltype(auto)
  dispatch_async_atlas(const std::chrono::steady_clock::time_point deadline,
                       const double *metrics, const size_t metrics_count,
                       Ret (^&&block)(Args...), Args &&... args) {
    const uint64_t type = work_type(block);
    return dispatch(deadline, metrics, metrics_count, type, [
      f_ = Block_copy(std::forward<decltype(block)>(block)),
      args_ = std::make_tuple(std::forward<Args>(args)...)
    ]() mutable { std::experimental::apply(std::move(f_), std::move(args_)); });
  }

  template <typename Ret, typename... Args>
  decltype(auto) dispatch_async(Ret (^&&f)(Args...), Args &&... args) {
    return dispatch([
      f_ = Block_copy(std::forward<decltype(f)>(f)),
      args_ = std::make_tuple(std::forward<Args>(args)...)
    ]() mutable { std::experimental::apply(std::move(f_), std::move(args_)); });
  }
#endif

};
}
#endif

