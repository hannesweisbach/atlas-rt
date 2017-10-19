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
#include <type_traits>
#include <experimental/tuple>

#include <iostream>

#else
#include <time.h>
#endif

#include <sched.h>

#ifdef __BLOCKS__
#include <Block.h>
#endif

#include "atlas/atlas-clock.h"

#ifdef __cplusplus
namespace atlas {

namespace _ {

template <typename T> uint64_t work_type(const T &) {
  const auto type = std::type_index(typeid(T)).hash_code();
  return type;
}

template <typename Ret, typename... Args>
uint64_t work_type(Ret (*&f)(Args...)) {
  return reinterpret_cast<uint64_t>(f);
}

template <typename Ret, typename... Args>
uint64_t work_type(Ret (&f)(Args...)) {
  return reinterpret_cast<uint64_t>(&f);
}

#ifdef __BLOCKS__
template <typename Ret, typename... Args>
uint64_t work_type(Ret (^f)(Args...)) {
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
#endif
}

class dispatch_queue {
protected:
  struct impl;
  std::unique_ptr<impl> d_;

  std::future<void> dispatch(const clock::time_point,
                             const double *, const size_t, const uint64_t,
                             std::function<void()>) const;
  std::future<void> dispatch(std::function<void()>, const uint64_t) const;

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

  template <typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>,
            typename = typename std::enable_if<
                std::is_move_assignable<Func>::value>::type>
  decltype(auto) async(const clock::time_point deadline, const double *metrics,
                       const size_t metrics_count, Func &&block,
                       Args &&... args) {
    const uint64_t type = _::work_type(block);
    return dispatch(deadline, metrics, metrics_count, type, [
      f_ = std::forward<Func>(block),
      args_ = std::make_tuple(std::forward<Args>(args)...)
    ]() mutable { std::experimental::apply(std::move(f_), std::move(args_)); });
  }

  template <typename Func, typename... Args,
            typename = std::result_of<Func &(Args...)>>
  decltype(auto) async(const clock::time_point deadline, const double *metrics,
                       const size_t metrics_count, Func const &block,
                       Args &&... args) {
    const uint64_t type = _::work_type(block);
    return dispatch(deadline, metrics, metrics_count, type, [
      f_ = block,
      args_ = std::make_tuple(std::forward<Args>(args)...)
    ]() mutable { std::experimental::apply(std::move(f_), std::move(args_)); });
  }

  template <typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>>
  decltype(auto) sync(const clock::time_point deadline, const double *metrics,
                      const size_t metrics_count, Func &&block,
                      Args &&... args) {
    return async(deadline, metrics, metrics_count, std::forward<Func>(block),
                 std::forward<Args>(args)...)
        .get();
  }

  /* No metrics overloads */
  template <typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>>
  decltype(auto) async(const clock::time_point deadline, Func &&block,
                       Args &&... args) {
    return async(deadline, static_cast<const double *>(nullptr), size_t(0),
                 std::forward<Func>(block), std::forward<Args>(args)...);
  }

  template <typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>>
  decltype(auto) sync(const clock::time_point deadline, Func &&block,
                      Args &&... args) {
    return sync(deadline, static_cast<const double *>(nullptr), size_t(0),
                std::forward<Func>(block), std::forward<Args>(args)...);
  }

  /* Overloads for relative deadlines */
  template <typename Rep, typename Period, typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>>
  decltype(auto) async(const std::chrono::duration<Rep, Period> deadline,
                       const double *metrics, const size_t metrics_count,
                       Func &&block, Args &&... args) {
    return async(clock::now() + deadline, metrics, metrics_count,
                 std::forward<Func>(block), std::forward<Args>(args)...);
  }

  template <typename Rep, typename Period, typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>>
  decltype(auto) sync(const std::chrono::duration<Rep, Period> deadline,
            const double *metrics, const size_t metrics_count, Func &&block,
            Args &&... args) {
    return sync(clock::now() + deadline, metrics, metrics_count,
                std::forward<Func>(block), std::forward<Args>(args)...);
  }

  /* Overloads for relative deadlines + no metrics */
  template <typename Rep, typename Period, typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>>
  decltype(auto) async(const std::chrono::duration<Rep, Period> deadline,
                       Func &&block, Args &&... args) {
    return async(clock::now() + deadline, static_cast<const double *>(nullptr),
                 size_t(0), std::forward<Func>(block),
                 std::forward<Args>(args)...);
  }

  template <typename Rep, typename Period, typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>>
  decltype(auto) sync(const std::chrono::duration<Rep, Period> deadline,
                      Func &&block, Args &&... args) {
    return sync(clock::now() + deadline, static_cast<const double *>(nullptr),
                size_t(0), std::forward<Func>(block),
                std::forward<Args>(args)...);
  }

  template <typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>>
  decltype(auto) async(Func &&f, Args &&... args) {
    return dispatch(
        [
          f_ = std::forward<Func>(f),
          args_ = std::make_tuple(std::forward<Args>(args)...)
        ]() mutable {
          std::experimental::apply(std::move(f_), std::move(args_));
        },
        _::work_type(f));
  }

  template <typename Func, typename... Args,
            typename = std::result_of_t<Func(Args...)>>
  auto sync(Func &&f, Args &&... args) {
    return async(std::forward<Func>(f), std::forward<Args>(args)...).get();
  }

#ifdef __BLOCKS__
  template <typename Ret, typename... Args>
  decltype(auto) async(const clock::time_point deadline, const double *metrics,
                       const size_t metrics_count, Ret (^block)(Args...),
                       Args &&... args) {
    const uint64_t type = _::work_type(block);
    return dispatch(deadline, metrics, metrics_count, type, [
      f_ = Block_copy(block),
      args_ = std::make_tuple(std::forward<Args>(args)...)
    ]() mutable { std::experimental::apply(std::move(f_), std::move(args_)); });
  }

  template <typename Ret, typename... Args>
  decltype(auto) sync(const clock::time_point deadline, const double *metrics,
                      const size_t metrics_count, Ret (^block)(Args...),
                      Args &&... args) {
    const uint64_t type = _::work_type(block);
    return dispatch(deadline, metrics, metrics_count, type, [
      f_ = Block_copy(block),
      args_ = std::make_tuple(std::forward<Args>(args)...)
    ]() mutable { std::experimental::apply(std::move(f_), std::move(args_)); }).get();
  }

  /* No metrics overload */
  template <typename Ret, typename... Args>
  decltype(auto) async(const clock::time_point deadline, Ret (^block)(Args...),
                       Args &&... args) {
    return async(deadline, static_cast<const double *>(nullptr), size_t(0),
                 block, std::forward<Args>(args)...);
  }

  /* Overload for relative deadlines */
  template <typename Rep, typename Period, typename Ret, typename... Args>
  decltype(auto) async(const std::chrono::duration<Rep, Period> deadline,
                       const double *metrics, const size_t metrics_count,
                       Ret (^block)(Args...), Args &&... args) {
    return async(clock::now() + deadline, metrics, metrics_count, block,
                 std::forward<Args>(args)...);
  }

  /* Overload for relative deadline + no metrics */
  template <typename Rep, typename Period, typename Ret, typename... Args>
  decltype(auto) async(const std::chrono::duration<Rep, Period> deadline,
                       Ret (^block)(Args...), Args &&... args) {
    return async(clock::now() + deadline, static_cast<const double *>(nullptr),
                 size_t(0), block, std::forward<Args>(args)...);
  }

  template <typename Ret, typename... Args>
  decltype(auto) async(Ret (^f)(Args...), Args &&... args) {
    return dispatch(
        [
          f_ = Block_copy(std::forward<decltype(f)>(f)),
          args_ = std::make_tuple(std::forward<Args>(args)...)
        ]() mutable {
          std::experimental::apply(std::move(f_), std::move(args_));
        },
        _::work_type(f));
  }
#endif

  static dispatch_queue &dispatch_get_main_queue();
  static void dispatch_main();
  static void dispatch_main_quit();
};

}
#endif

