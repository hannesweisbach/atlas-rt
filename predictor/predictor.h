#pragma once

#include <memory>
#include <chrono>

namespace atlas {
class estimator {
  struct impl;
  std::unique_ptr<impl> d_;

public:
  estimator();
  ~estimator();

  std::chrono::nanoseconds predict(const uint64_t job_type, const uint64_t id,
                                   const double *metrics, const size_t count);
  void train(const uint64_t job_type, const uint64_t id,
             const std::chrono::nanoseconds exectime);
};
}
