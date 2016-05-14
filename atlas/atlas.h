#pragma once

#ifdef __cplusplus
#include <chrono>
#include <thread>
#include <pthread.h>
#include <algorithm>
#include <sstream>
#include <stdexcept>

#include <cerrno>
#include <cstring>
#else
#include <errno.h>
#endif

#include <unistd.h>
#include <sys/types.h>

#if defined(__x86_64__)
#define SYS_atlas_next 323
#define SYS_atlas_submit 324
#define SYS_atlas_update 325
#define SYS_atlas_remove 326
#define SYS_atlas_tp_create 327
#define SYS_atlas_tp_destroy 328
#define SYS_atlas_tp_join 329
#define SYS_atlas_tp_submit 330
#define ARG64(x) x
#elif defined(__i386__)
#define SYS_atlas_next 359
#define SYS_atlas_submit 360
#define SYS_atlas_update 361
#define SYS_atlas_remove 362
#elif defined(__arm__)
#define ARG64(x) static_cast<uint32_t>(x >> 32), static_cast<uint32_t>(x & ~0)
#else
#error Architecture not supported.
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline long atlas_submit(pid_t tid, uint64_t id,
                                const struct timeval *const exectime,
                                const struct timeval *const deadline) {
#if defined(__x86_64__)
  return syscall(SYS_atlas_submit, tid, id, exectime, deadline);
#elif defined(__arm__)
  return syscall(SYS_atlas_submit, tid, 0, ARG64(id), exectime, deadline);
#endif
}

static inline long atlas_next(uint64_t *next) {
  for(;; ) {
    long ret = syscall(SYS_atlas_next, next);
    if(ret != -1 || errno != EINTR)
      return ret;
  }
}

static inline long atlas_remove(pid_t tid, const uint64_t id) {
#if defined(__x86_64__)
  return syscall(SYS_atlas_remove, tid, id);
#elif defined(__arm__)
  return syscall(SYS_atlas_remove, tid, 0, ARG64(id));
#endif
}

static inline long atlas_update(pid_t tid, uint64_t id,
                                const struct timeval *const exectime,
                                const struct timeval *const deadline) {
#if defined(__x86_64__)
  return syscall(SYS_atlas_update, tid, id, exectime, deadline);
#elif defined(__arm__)
  return syscall(SYS_atlas_update, tid, 0, ARG64(id), exectime, deadline);
#endif
}

static inline long atlas_tp_create(uint64_t *id) {
  return syscall(SYS_atlas_tp_create, id);
}

static inline long atlas_tp_destroy(const uint64_t id) {
  return syscall(SYS_atlas_tp_destroy, ARG64(id));
}

static inline long atlas_tp_join(const uint64_t id) {
  return syscall(SYS_atlas_tp_join, ARG64(id));
}

static inline long atlas_tp_submit(const uint64_t tpid, const uint64_t id,
                                   const struct timeval *const exectime,
                                   const struct timeval *const deadline) {
  return syscall(SYS_atlas_tp_submit, ARG64(tpid), ARG64(id), exectime,
                 deadline);
}

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

namespace atlas {

static inline decltype(auto) submit(pid_t tid, uint64_t id,
                                    const struct timeval *const exectime,
                                    const struct timeval *const deadline) {
  return atlas_submit(tid, id, exectime, deadline);
}

static inline decltype(auto) update(pid_t tid, uint64_t id,
                                    const struct timeval *const exectime,
                                    const struct timeval *const deadline) {
  return atlas_update(tid, id, exectime, deadline);
}

static inline decltype(auto) remove(pid_t tid, const uint64_t id) {
  return atlas_remove(tid, id);
}

static inline decltype(auto) next(uint64_t &next) { return atlas_next(&next); }

[[deprecated]] static inline decltype(auto) next() {
  uint64_t dummy;
  return atlas_next(&dummy);
}

namespace {
template <class Rep, class Period>
struct timeval to_timeval(const std::chrono::duration<Rep, Period> &duration) {
  using namespace std::chrono;
  auto secs = duration_cast<seconds>(duration);
  auto usecs = duration_cast<microseconds>(duration - secs);
  return {
      static_cast<time_t>(secs.count()),
      static_cast<suseconds_t>(usecs.count()),
  };
}

template <class Clock, class Duration>
struct timeval to_timeval(const std::chrono::time_point<Clock, Duration> &t) {
  using namespace std::chrono;
  auto secs = time_point_cast<seconds>(t);
  auto usecs = duration_cast<microseconds>(t - secs);

  return {
      static_cast<time_t>(secs.time_since_epoch().count()),
      static_cast<suseconds_t>(usecs.count()),
  };
}
}

namespace threadpool {
static inline auto create() {
  uint64_t id;
  auto ret = syscall(SYS_atlas_tp_create, &id);
  if (ret != 0) {
    std::ostringstream os;
    os << "Error creating threadpool (" << errno << "): " << strerror(errno);
    throw std::runtime_error(os.str());
  }
  return id;
}

static inline decltype(auto) destroy(const uint64_t id) {
  return syscall(SYS_atlas_tp_destroy, ARG64(id));
}

static inline decltype(auto) join(const uint64_t id) {
  return syscall(SYS_atlas_tp_join, ARG64(id));
}

static inline decltype(auto) submit(const uint64_t tpid, const uint64_t id,
                                    const struct timeval *const exectime,
                                    const struct timeval *const deadline) {
  return syscall(SYS_atlas_tp_submit, ARG64(tpid), ARG64(id), exectime,
                 deadline);
}

template <class Rep1, class Period1, class Rep2, class Period2>
decltype(auto) submit(const uint64_t tpid, const uint64_t id,
                      const std::chrono::duration<Rep1, Period1> exec_time,
                      const std::chrono::duration<Rep2, Period2> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline =
      to_timeval(std::chrono::steady_clock::now() + deadline);

  return threadpool::submit(tpid, id, &tv_exectime, &tv_deadline);
}

template <class Rep, class Period, class Clock>
decltype(auto) submit(const uint64_t tpid, const uint64_t id,
                      const std::chrono::duration<Rep, Period> exec_time,
                      const std::chrono::time_point<Clock> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline = to_timeval(deadline);

  return threadpool::submit(tpid, id, &tv_exectime, &tv_deadline);
}
}

template <class Rep1, class Period1, class Rep2, class Period2>
decltype(auto) submit(pid_t tid, uint64_t id,
                      std::chrono::duration<Rep1, Period1> exec_time,
                      std::chrono::duration<Rep2, Period2> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline =
      to_timeval(std::chrono::steady_clock::now() + deadline);

  return atlas_submit(tid, id, &tv_exectime, &tv_deadline);
}

template <class Rep, class Period, class Clock, class Duration>
decltype(auto) submit(pid_t tid, uint64_t id,
                      std::chrono::duration<Rep, Period> exec_time,
                      std::chrono::time_point<Clock, Duration> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline = to_timeval(deadline);

  return atlas_submit(tid, id, &tv_exectime, &tv_deadline);
}

template <class Rep1, class Period1, class Rep2, class Period2>
decltype(auto) update(pid_t tid, uint64_t id,
                      std::chrono::duration<Rep1, Period1> exec_time,
                      std::chrono::duration<Rep2, Period2> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline =
      to_timeval(std::chrono::steady_clock::now() + deadline);

  return atlas_update(tid, id, &tv_exectime, &tv_deadline);
}

template <class Rep, class Period, class Clock, class Duration>
decltype(auto) update(pid_t tid, uint64_t id,
                      std::chrono::duration<Rep, Period> exec_time,
                      std::chrono::time_point<Clock, Duration> deadline) {
  struct timeval tv_exectime = to_timeval(exec_time);
  struct timeval tv_deadline = to_timeval(deadline);

  return atlas_update(tid, id, &tv_exectime, &tv_deadline);
}

template <class Rep1, class Period1>
decltype(auto) update(pid_t tid, uint64_t id,
                      std::chrono::duration<Rep1, Period1> exec_time) {
  struct timeval tv_exectime = to_timeval(exec_time);

  return atlas_update(tid, id, &tv_exectime, nullptr);
}

template <class Clock, class Duration>
decltype(auto) update(pid_t tid, uint64_t id,
                      std::chrono::time_point<Clock, Duration> deadline) {
  struct timeval tv_deadline = to_timeval(deadline);

  return atlas_update(tid, id, nullptr, &tv_deadline);
}

namespace np {

static inline pid_t from(const pthread_t tid) {
  /* (byte) offset of the pthread.tid member from glibc */
#if defined(__x86_64__)
  const size_t offset = 720;
#elif defined(__arm__)
  const size_t offset = 104;
#else
#error "Architecture not supported."
#endif
  pid_t result;
  pid_t *src = reinterpret_cast<pid_t *>(reinterpret_cast<char *>(tid) + offset);
  std::copy(src, src + 1, &result);
  if (!result) {
    std::ostringstream os;
    os << "Thread " << tid << " has either not started or already quit.";
    throw std::runtime_error(os.str());
  }
  return result;
}

static inline pid_t from(const std::thread::id tid) {
  /* std::thread::id has a pthread_t as first member in libc++;
   * native_handle() does not exist in std::this_thread namespace */
  return from(*reinterpret_cast<const pthread_t *>(&tid));
}

static inline decltype(auto) from(std::thread &thread) {
  return from(thread.native_handle());
}

static inline auto from(const pid_t &tid) { return tid; }

template <typename Handle, class Rep1, class Period1, class Rep2, class Period2>
decltype(auto) submit(Handle &tid, uint64_t id,
                      std::chrono::duration<Rep1, Period1> exec_time,
                      std::chrono::duration<Rep2, Period2> deadline) {
  return atlas::submit(from(tid), id, exec_time, deadline);
}

template <typename Handle, class Rep, class Period, class Clock, class Duration>
decltype(auto) submit(Handle &tid, uint64_t id,
                      std::chrono::duration<Rep, Period> exec_time,
                      std::chrono::time_point<Clock, Duration> deadline) {
  return atlas::submit(from(tid), id, exec_time, deadline);
}

template <typename Handle>
static inline decltype(auto) remove(const Handle &tid, uint64_t id) {
  return atlas_remove(from(tid), id);
}

template <typename Handle, class Rep1, class Period1, class Rep2, class Period2>
decltype(auto) update(const Handle &handle, uint64_t id,
                      std::chrono::duration<Rep1, Period1> exec_time,
                      std::chrono::duration<Rep2, Period2> deadline) {
  return atlas::update(from(handle), id, exec_time, deadline);
}

template <typename Handle, class Rep, class Period>
decltype(auto) update(const Handle &tid, uint64_t id,
                      std::chrono::duration<Rep, Period> exec_time) {
  struct timeval tv_exectime = to_timeval(exec_time);
  return atlas::update(from(tid), id, &tv_exectime, nullptr);
}

template <typename Handle, class Clock, class Duration>
decltype(auto) update(const Handle &tid, uint64_t id,
                      std::chrono::time_point<Clock, Duration> deadline) {
  struct timeval tv_deadline = to_timeval(deadline);

  return atlas::update(from(tid), id, nullptr, &tv_deadline);
}
}
}

#endif /* __cplusplus */
