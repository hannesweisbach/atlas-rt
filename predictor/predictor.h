#pragma once

#include <memory>
#include <chrono>

#include <cstdlib>

namespace atlas {
class estimator {
  struct impl;
  std::unique_ptr<impl> d_;

public:
  estimator(const char *fname = std::getenv("ATLAS_PREDICTOR"));
  ~estimator();

  std::chrono::nanoseconds predict(const uint64_t job_type, const uint64_t id,
                                   const double *metrics, const size_t count);
  void train(const uint64_t job_type, const uint64_t id,
             const std::chrono::nanoseconds exectime);
  void save(const char *fname = std::getenv("ATLAS_PREDICTOR")) const;
  bool operator==(const estimator &rhs) const;
};
}
