#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
#include <array>

#include <boost/math/common_factor_rt.hpp>

#include "taskgen.h"

static std::mt19937_64 generator;

template <typename Rep, typename Res>
static auto uunisort(size_t n, utilization<Rep, Res> usum,
                     const utilization<Rep, Res> umax) {
  std::vector<utilization<Rep, Res>> utilizations(n);
  using U = utilization<Rep, Res>;

  for (; n > 1; --n) {
    auto ulower = std::max(usum - static_cast<long>(n - 1) * umax, U{1});
    auto uupper = std::min(usum - U{static_cast<long>(n - 1)}, umax);
    std::uniform_int_distribution<Rep> distribution(ulower.utilization,
                                                    uupper.utilization);
    auto u = utilization<Rep, Res>{distribution(generator)};

    usum -= u;
#if 0
    std::cout << n << " [" << ulower.utilization << ", " << uupper.utilization
              << "] -> " << u.utilization << ", " << usum.utilization
              << std::endl;
#endif
    utilizations.at(n - 1) = u;
  }

  utilizations.at(0) = usum;

  return utilizations;
}

#if 0
template <typename Rep, typename Res>
static auto uunisort(const size_t n, utilization<Rep, Res> u) {
  std::uniform_int_distribution<Rep> distribution(0, u.utilization);

  std::vector<utilization<Rep, Res>> utilizations(n);
  std::generate_n(std::begin(utilizations), n - 1, [&distribution] {
    return utilization<Rep, Res>{distribution(generator)};
  });
  utilizations.back() = u;

  std::sort(std::begin(utilizations), std::end(utilizations));
  std::adjacent_difference(std::begin(utilizations), std::end(utilizations),
                           std::begin(utilizations));

  return utilizations;
}

static auto uunisort(const size_t n, const double u) {
  std::uniform_int_distribution<uint64_t> distribution(
      0, static_cast<uint64_t>(u * 1000.0));

  std::vector<double> utilizations(n);
  std::generate_n(std::begin(utilizations), n - 1,
                  [&distribution] { return distribution(generator) / 1000.0; });
  utilizations.back() = u;

  std::sort(std::begin(utilizations), std::end(utilizations));
  std::adjacent_difference(std::begin(utilizations), std::end(utilizations),
                           std::begin(utilizations));

  return utilizations;
}
#endif

std::ostream &operator<<(std::ostream &os, const task_attr &t) {
  using namespace std::chrono;
  /* 10th of 1 ms */
  const auto e =
      duration_cast<std::chrono::duration<float, std::ratio<1, 10000>>>(t.e) /
      10.0;
  const auto p =
      duration_cast<std::chrono::duration<float, std::ratio<1, 10000>>>(t.p) /
      10.0;
  return os << "e: " << e.count() << ", p: " << p.count()
            << ", u: " << t.utilization;
}

std::ostream &operator<<(std::ostream &os, const std::vector<task_attr> &ts) {
  auto usum = U{0};
  for (const auto &t : ts) {
    os << t << std::endl;
    usum += t.utilization;
  }
  return os << "utilization: " << usum << std::endl;
}

#if 0
task_set generate_taskset(const size_t n, const double u) {
  const auto utilizations = uunisort(n, u);
  task_set ts(n);

  std::transform(
}
#endif

template <typename T1, typename T2, typename T = std::common_type_t<T1, T2>>
static T draw_from(const T1 a, const T2 b) {
  std::uniform_int_distribution<T> distribution(a, b);
  return distribution(generator);
}

using bin_t = std::pair<period, period>;
static decltype(auto) draw_from(const bin_t &bin) {
  return draw_from(bin.first.count(), bin.second.count());
}

static auto generate_period() {
  static constexpr std::array<int64_t, 25> periods{
      {16, 24, 32, 36, 48, 24, 36, 48, 54, 72, 32, 48, 64, 72, 96, 36, 54, 72,
       81, 108, 48, 72, 96, 108, 144}};
  return period{periods[draw_from(0UL, periods.size() - 1)]};
}

template <typename Rep, typename Res>
auto generate_taskset_(const size_t n, const utilization<Rep, Res> usum,
                       const utilization<Rep, Res> umax, const period &p_min,
                       const period &p_max) {
  using namespace std::chrono;
  if (p_min > p_max)
    throw std::runtime_error("p_min larger than p_max");
  if (p_min <= period{0})
    throw std::runtime_error("p_min less than 0");
  if (umax * static_cast<long>(n) < usum)
    throw std::runtime_error("n*umax < usum");
  const auto bin_count =
      static_cast<size_t>(std::ceil(std::log10(p_max / p_min)));
  std::vector<bin_t> bins(bin_count);

  {
    if (bin_count == 0) {
      bins.push_back(std::make_pair(p_min, p_max));
    } else if (bin_count == 1) {
      bins.front() = std::make_pair(p_min, p_max);
    } else {
      auto upper = p_min * 10;
      bins.front() = std::make_pair(p_min, upper);

      for (size_t i = 1; i < bin_count - 1; ++i) {
        const auto lower = upper + period{1};
        upper = upper * 10;
        bins.at(i) = std::make_pair(lower, upper);
      }
      bins.back() = std::make_pair(upper + period{1}, p_max);
    }

#ifdef DEBUG_BINS
    std::cout << "Bin count: " << bin_count << std::endl;
    for (const auto &bin : bins) {
      std::cout << "(" << std::setw(10) << bin.first.count() << ", "
                << std::setw(10) << bin.second.count() << ")" << std::endl;
    }
#endif
  }

  const auto utilizations = uunisort(n, usum, umax);
  const auto all = (bin_count > n) ? n / bin_count * std::min(n, bin_count) : 0;
  std::vector<task_attr> ts(n);

  size_t i = 0;
  auto next = std::transform(
      std::begin(utilizations),
      std::begin(utilizations) + static_cast<long>(all), std::begin(ts),
      [&bins, &i](const auto &u) {
#define COOKED_PERIODS
#ifdef COOKED_PERIODS
        const auto p = generate_period();
#else
        const auto p = period{draw_from(bins.at(i++ % bins.size()))};
#endif
        // period [us] * utilization [milli] -> execution time [ns]
        const auto e = execution_time{duration_cast<microseconds>(p).count() *
                                      u.utilization};
        return task_attr{e, p, u};
      });

  std::transform(std::begin(utilizations) + static_cast<long>(all),
                 std::end(utilizations), next, [&bins](const auto &u) {
#ifdef COOKED_PERIODS
                   const auto p = generate_period();
#else
                   const auto bin = draw_from(0U, bins.size() - 1);
                   const auto p = period{draw_from(bins.at(bin))};
#endif
                   const auto e = execution_time{
                       duration_cast<microseconds>(p).count() * u.utilization};
                   return task_attr{e, p, u};
                 });

  return ts;
}

std::vector<task_attr> generate_taskset(const size_t n, const U usum,
                                        const U umax, const period &p_min,
                                        const period &p_max) {
  return generate_taskset_(n, usum, umax, p_min, p_max);
}

#if 0
int main() {
  using namespace std::chrono;
  std::cout << std::fixed;
#if 0
  for (size_t i = 0;; ++i) {
    auto us = uunisort(3, 0.5);
    const auto sum =  std::accumulate(std::begin(us), std::end(us), 0.0);
    if (sum != 0.5) {
      std::cout << i << " ";
      for (const auto &u : us)
        std::cout << u << " ";
      std::cout << std::setprecision(26) << sum << std::endl;
    }
  }
#else
#if 0
  for (size_t i = 0; i < 5000; ++i) {
    auto usum = U{1500};
    auto us = uunisort(3, usum, U{1000});
    for (const auto &u : us)
      std::cout << u << " ";
    std::cout << std::endl;
    const auto sum = std::accumulate(std::begin(us), std::end(us), U{0});
    if (sum != usum) {
      std::cout << i << " ";
      for (const auto &u : us)
        std::cout << u << " ";
      std::cout << std::setprecision(26) << sum << std::endl;
    }
  }

  return 0;
#endif
#endif
  for (int i = 0; i < 100; ++i) {
    std::cout << std::fixed;
    auto ts = generate_taskset(8, U{2000}, U{500}, 1ms, 1000ms);
   std::cout << ts << std::endl;
  }

}
#endif

hyperperiod_t hyperperiod(const std::vector<task_attr> &tasks) {
  auto counts = std::accumulate(
      std::begin(tasks), std::end(tasks), hyperperiod_t::rep{1},
      [](const auto &hp, const auto &task) {
        return boost::math::lcm(
            hp, std::chrono::duration_cast<hyperperiod_t>(task.p).count());
      });

  return hyperperiod_t{counts};
}

