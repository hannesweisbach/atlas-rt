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
    atlas::submit(tid, job, 1s, 1s);
  }

  for (size_t i = 0; i < count; ++i) {
    auto s_start = steady_clock::now();
    atlas::submit(tid, i + jobs, 1s, s_start + 1s);
    auto s_end = steady_clock::now();

    submits[i] = duration_cast<nanoseconds>(s_end - s_start).count();

    auto n_start = steady_clock::now();
    atlas::next();
    auto n_end = steady_clock::now();

    nexts[i] = duration_cast<nanoseconds>(n_end - n_start).count();
  }

  return std::make_pair(submits, nexts);
}

static auto nth_element(const std::vector<int64_t> &data, const double nth) {
  return data.at(static_cast<size_t>((data.size() - 1) * nth));
}

static void output(std::vector<int64_t> &data, const bool sort, const bool all) {
  if (sort || !all) {
    std::sort(std::begin(data), std::end(data));
  }

  if (all) {
    for (const auto val : data)
      std::cout << val << std::endl;
  } else {
    std::cout << "#  minimum    1st %ile      median   99th %ile     maximum"
              << std::endl;
    std::cout << std::setw(10) << data.front() << std::setw(12)
              << nth_element(data, 0.01) << std::setw(12)
              << nth_element(data, 0.5) << std::setw(12)
              << nth_element(data, 0.99) << std::setw(12) << data.back()
              << std::endl;
  }
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;

  unsigned jobs;
  size_t count;
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
    ("all", "Output all numbers. (instead of short statistic");
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

  output(vm.count("next") ? nexts : submits, vm.count("sort"), vm.count("all"));

  return EXIT_SUCCESS;
}
