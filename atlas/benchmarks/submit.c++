#include <chrono>
#include <thread>
#include <utility>
#include <iostream>
#include <iomanip>

#include <boost/program_options.hpp>

#include "atlas/atlas.h"
#include "common/common.h"

static auto run(unsigned jobs, size_t count) {
  using namespace std::chrono;
  const auto tid = gettid();
  std::vector<int64_t> submits(count, 0);
  std::vector<int64_t> nexts(count, 0);

  for (unsigned job = 0; job < jobs; ++job) {
    atlas::submit(tid, job, 1s, 2s);
  }

  for (size_t i = 0; i < count; ++i) {
    auto s_start = steady_clock::now();
    atlas::submit(tid, i + jobs, 1s, s_start + 2s);
    auto s_end = steady_clock::now();

    submits[i] = duration_cast<nanoseconds>(s_end - s_start).count();

    auto n_start = steady_clock::now();
    atlas::next();
    auto n_end = steady_clock::now();

    nexts[i] = duration_cast<nanoseconds>(n_end - n_start).count();
  }

  return std::make_pair(submits, nexts);
}

static auto nth_element(const std::vector<int64_t> &data, const size_t nth) {
  return data.at(static_cast<size_t>((data.size() - 1) * nth / 100.0));
}

static const char *ordstr(const size_t ord) {
  switch (ord) {
  case 1:
    return "st";
  case 2:
    return "nd";
  case 3:
    return "rd";
  default:
    return "th";
  }
}

static void output(std::vector<int64_t> &data, const bool sort, const bool all,
                   const size_t quantiles) {
  if (sort || !all) {
    std::sort(std::begin(data), std::end(data));
  }

  if (all) {
    for (const auto val : data)
      std::cout << val << std::endl;
  } else {
    auto quantile = quantiles ? 100 / quantiles : 100;
    std::cout << "#   minimum";

    for (size_t q = quantile; q < 99; q += quantile) {
      std::cout << "   " << std::setw(2) << q << ordstr(q) << "%-tile";
    }
    std::cout << "   99th%-ile     maximum" << std::endl;

    std::cout << std::setw(11) << data.front();
    for (size_t q = quantile; q < 99; q += quantile) {
      std::cout << std::setw(13) << nth_element(data, q);
    }
    std::cout << std::setw(12) << nth_element(data, 99);
    std::cout << std::setw(12) << data.back() << std::endl;
  }
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;

  unsigned jobs;
  size_t count;
  size_t quantiles;
  po::options_description desc("Benchmark suite for atlas::submit() and "
                               "atlas::next(). Measures delays in nanoseconds");

  // clang-format off
  desc.add_options()
    ("help", "Produce help message")
    ("jobs", po::value(&jobs)->default_value(0),
     "Number of jobs in the job queue. (Default: 0)")
    ("count", po::value(&count)->default_value(100),
     "Number of measurements to make. (Default: 100)")
    ("next", "Output numbers for atlas::next() instead of atlas::submit(). "
     "(Default: atlas::submit())")
    ("sort", "Output numbers sorted.")
    ("all", "Output all numbers. (instead of short statistic")
    ("quantiles", po::value(&quantiles)->default_value(2),
     "Number of quantiles, i.e. 4 for 25% quantiles.");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc;
    return EXIT_SUCCESS;
  }

  std::vector<int64_t> submits;
  std::vector<int64_t> nexts;

  std::tie(submits, nexts) = run(jobs, count);

  output(vm.count("next") ? nexts : submits, vm.count("sort"), vm.count("all"),
         quantiles);

  return EXIT_SUCCESS;
}
