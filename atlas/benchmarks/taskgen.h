#include <chrono>
#include <iostream>
#include <utility>
#include <vector>

#include <boost/multiprecision/gmp.hpp>

template <typename Rep, typename Res> struct utilization {
  using rep = Rep;
  using res = Res;
  Rep utilization;

  //struct utilization &operator=(const struct utilization &) = default;

  bool operator<(const struct utilization &rhs) const {
    return utilization < rhs.utilization;
  }

  bool operator==(const struct utilization &rhs) const {
    return utilization == rhs.utilization;
  }

  bool operator!=(const struct utilization &rhs) const {
    return !(*this == rhs);
  }

  struct utilization operator-(const struct utilization &rhs) const {
    return {utilization - rhs.utilization};
  }
  
  struct utilization operator+(const struct utilization &rhs) const {
    return {utilization + rhs.utilization};
  }

  struct utilization &operator+=(const struct utilization &rhs) {
    *this = *this + rhs;
    return *this;
  }

  struct utilization &operator-=(const struct utilization &rhs) {
    *this = *this - rhs;
    return *this;
  }
};

template <typename Rep, typename Res, typename Scalar,
          typename Ret = std::common_type_t<Rep, Scalar>>
auto operator*(const Scalar &lhs, const utilization<Rep, Res> &rhs) {
  return utilization<Ret, Res>{lhs * rhs.utilization};
}

template <typename Rep, typename Res, typename Scalar>
decltype(auto) operator*(const utilization<Rep, Res> &lhs, const Scalar &rhs) {
  return rhs * lhs;
}

template <typename Rep, typename Res>
std::ostream &operator<<(std::ostream &os, const utilization<Rep, Res> &u) {
  return os << static_cast<double>(u.utilization) / Res::den;
}

using U = utilization<int64_t, std::milli>;

using ns = std::chrono::duration<boost::multiprecision::mpz_int, std::nano>;
using us = std::chrono::duration<boost::multiprecision::mpz_int, std::micro>;
using ms = std::chrono::duration<boost::multiprecision::mpz_int, std::milli>;
using s = std::chrono::duration<boost::multiprecision::mpz_int, std::ratio<1>>;
using execution_time = std::chrono::nanoseconds;
using period = std::chrono::milliseconds;
using hyperperiod_t = ns;

struct task_attr {
  execution_time e;
  period p;
  U utilization;
  task_attr() = default;
  task_attr(const execution_time e_, const period &p_, const U &u_)
      : e(e_), p(p_), utilization(u_) {}
};

std::ostream &operator<<(std::ostream &os, const task_attr &t);
std::ostream &operator<<(std::ostream &os, const std::vector<task_attr> &t);

#if 0
task_set generate_taskset(const size_t n, const double u);
task_set generate_taskset(const size_t n, const double u,
                          const std::chrono::nanoseconds &p);
#endif
std::vector<task_attr> generate_taskset(const size_t n, U usum, U umax,
                                        const period &p_min,
                                        const period &p_max);

