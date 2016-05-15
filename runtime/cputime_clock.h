#pragma once

#include <chrono>
#include <stdexcept>
#include <string>

#include <cstring>
#include <cerrno>

class cputime_clock {
public:
  using rep = typename std::chrono::nanoseconds::rep;
  using period = typename std::chrono::nanoseconds::period;
  using duration = std::chrono::nanoseconds;
  using time_point = std::chrono::time_point<cputime_clock>;
  static constexpr bool is_steady = false;

  static time_point now() {
    struct timespec cputime;

    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &cputime)) {
      throw std::runtime_error("Error " + std::to_string(errno) + " " +
                               strerror(errno));
    }

    rep ns = static_cast<rep>(cputime.tv_nsec) +
             static_cast<rep>(cputime.tv_sec) * 1'000'000'000;
    return time_point(duration(ns));
  }
};

