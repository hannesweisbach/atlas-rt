#include <vector>
#include <utility>
#include <algorithm>
#include <deque>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include <assert.h>

#include "predictor.h"

extern "C" {
#include "llsp.h"
}

#include "llsp-internal.h"

[[noreturn]] static void throw_estimator_not_found(const uint64_t type) {
  std::ostringstream os;
  os << "Estimator for type " << std::hex << type << " not found.";
  throw std::runtime_error(os.str());
}

static bool operator==(const struct llsp_s &lhs, const struct llsp_s &rhs) {
  bool equal = true;
  if (lhs.metrics != rhs.metrics) {
    std::cout << "metrics differ" << std::endl;
    equal = false;
  }

  if ((lhs.data == nullptr) != (rhs.data == nullptr)) {
    std::cout << "Initialization state differs" << std::endl;
    return false;
  }

  const size_t metrics = lhs.metrics;
  const size_t data_size = (metrics + 1) * (metrics + 2) * sizeof(double);
  if (memcmp(lhs.data, rhs.data, data_size) != 0) {
    std::cout << "data differs" << std::endl;
    for (size_t i = 0; i < data_size / sizeof(double); ++i) {
      std::cout << lhs.data + i << " " << lhs.data[i] << " " << rhs.data + i
                << " " << rhs.data[i] << std::endl;
    }
    equal = false;
  }

  if (lhs.full.columns != rhs.full.columns ||
      lhs.sort.columns != rhs.sort.columns ||
      lhs.good.columns != rhs.good.columns) {
    std::cout << "column mismatch" << std::endl;
    equal = false;
  }

  {
    for (size_t i = 0; i < lhs.full.columns; ++i) {
      if (lhs.full.matrix[i] - lhs.data != rhs.full.matrix[i] - rhs.data) {
        std::cout << "full offsets differ " << i << " of " << lhs.full.columns
                  << std::endl;
        std::cout << lhs.data << " " << lhs.full.matrix[i] - lhs.data << "; "
                  << rhs.data << " " << rhs.full.matrix[i] - rhs.data << " "
                  << rhs.full.matrix[i] << std::endl;
        equal = false;
      }
    }
  }

  {
    for (size_t i = 0; i < lhs.sort.columns; ++i) {
      if (lhs.sort.matrix[i] - lhs.data != rhs.sort.matrix[i] - rhs.data) {
        std::cout << "sort offsets differ" << std::endl;
        equal = false;
      }
    }
  }

  {
    for (size_t i = 0; i < lhs.good.columns; ++i) {
      if (lhs.good.matrix[i] - lhs.data == rhs.good.matrix[i] - rhs.data) {
        continue;
      } else if (lhs.good.matrix[i] == lhs.good.matrix[lhs.good.columns - 1] &&
                 rhs.good.matrix[i] == rhs.good.matrix[rhs.good.columns - 1]) {
        continue;
      } else {
        std::cout << "good offsets differ: " << std::endl;
        std::cout << lhs.good.matrix[i] << " " << lhs.data << " ("
                  << lhs.good.matrix[i] - lhs.data << ") "
                  << lhs.good.matrix[lhs.good.columns - 1] << std::endl;
        std::cout << rhs.good.matrix[i] << " " << rhs.data << " ("
                  << rhs.good.matrix[i] - rhs.data << ") "
                  << rhs.good.matrix[rhs.good.columns - 1] << std::endl;
        equal = false;
      }
    }

    if (memcmp(lhs.good.matrix[metrics], rhs.good.matrix[metrics],
               (metrics + 2) * sizeof(double)) != 0) {
      std::cout << "extra differs" << std::endl;
      std::cout << lhs.good.matrix[metrics] << " " << rhs.good.matrix[metrics]
                << std::endl;
      std::cout << rhs.data << std::endl;
      equal = false;
    }
  }

  if (memcmp(&lhs.last_measured, &rhs.last_measured, sizeof(double)) != 0) {
    std::cout << "last_measured differ" << std::endl;
    equal = false;
  }

  if (memcmp(lhs.result, rhs.result, metrics) != 0) {
    std::cout << "result differ" << std::endl;
    equal = false;
  }

  return equal;
}

class llsp {
  struct llsp_disposer {
    void operator()(llsp_t *llsp) { llsp_dispose(llsp); }
  };
  std::shared_ptr<llsp_t> llsp_;

public:
  llsp(const size_t count) : llsp_(llsp_new(count + 1), llsp_disposer{}) {}
  void add(const double *metrics, double target) {
    llsp_add(llsp_.get(), metrics, target);
  }
  const double *solve() { return llsp_solve(llsp_.get()); }
  double predict(const double *metrics) {
    return llsp_predict(llsp_.get(), metrics);
  }

  bool operator==(const llsp& rhs) const {
    return *llsp_ == *rhs.llsp_;
  }
};

namespace atlas {

static constexpr std::chrono::nanoseconds
overallocation(std::chrono::nanoseconds prediction) {
  using namespace std::chrono;
  using namespace std::literals::chrono_literals;
  return (prediction > 1ms) ? (prediction * 1025) / 1000 : prediction + 25us;
}

struct estimator_ctx {
  uint64_t type;
  size_t count;
  class llsp llsp;

  struct job {
    uint64_t id;
    std::vector<double> metrics;
    std::chrono::nanoseconds prediction;

    job(const uint64_t id_, const double *metrics_, const size_t count_)
        : id(id_), metrics(count_ + 1, 1.0), prediction(0) {
      std::copy_n(metrics_, count_, std::begin(metrics));
      assert(metrics.at(count_) == 1.0);
    }
  };

  std::deque<job> jobs;

  auto remove(const uint64_t id) {
    auto it = std::find_if(std::begin(jobs), std::end(jobs),
                           [id](const auto &job) { return job.id == id; });
    if (it == std::end(jobs)) {
      std::ostringstream os;
      os << "Job " << std::hex << id << " for Estimator type " << type
         << " not found.";
      throw std::runtime_error(os.str());
    }

    auto job = std::move(*it);
    jobs.erase(it);

    return job;
  }

  bool operator==(const estimator_ctx &rhs) const {
    return type == rhs.type && count == rhs.count && llsp == rhs.llsp;
  }

  estimator_ctx(const uint64_t type_, const size_t count_)
      : type(type_), count(count_), llsp(count) {}
};

struct estimator::impl {
  std::vector<estimator_ctx> estimators;
  mutable std::mutex lock;

  auto do_find(uint64_t type) {
    auto it =
        std::lower_bound(std::begin(estimators), std::end(estimators), type,
                         [](const auto &estimator, const uint64_t type_) {
                           return estimator.type < type_;
                         });

    if (it == std::end(estimators))
      throw_estimator_not_found(type);

    return it;
  }

  estimator_ctx &find(uint64_t type) {
    auto it = do_find(type);
    if (it->type != type)
      throw_estimator_not_found(type);
    return *it;
  }

  estimator_ctx &find_insert(uint64_t type, size_t count) {
    try {
      auto it = do_find(type);
      if (it->type == type)
        return *it;
      else
        return *estimators.insert(it, {type, count});
    } catch (const std::runtime_error &) {
      return *estimators.insert(std::end(estimators), {type, count});
    }
  }

  std::chrono::nanoseconds predict(estimator_ctx &estimator) {
    using namespace std::chrono;
    assert(estimator.jobs.size() > 0);
    auto &job = estimator.jobs.back();
    job.prediction = duration_cast<nanoseconds>(
        duration<double>(estimator.llsp.predict(job.metrics.data())));

    return job.prediction;
  }

  bool operator==(const impl &rhs) const {
    return std::equal(estimators.begin(), estimators.end(),
                      rhs.estimators.begin());
  }

  impl() {}
  ~impl() {}
};

estimator::estimator() : d_(std::make_unique<impl>()) {}
estimator::~estimator() = default;
std::chrono::nanoseconds estimator::predict(const uint64_t job_type,
                                            const uint64_t id,
                                            const double *metrics,
                                            const size_t count) {
  std::lock_guard<std::mutex> l(d_->lock);
  auto &estimator = d_->find_insert(job_type, count);

  {
    estimator.jobs.emplace_back(id, metrics, count);
    const auto prediction = d_->predict(estimator);
    return overallocation(prediction);
  }
}

void estimator::train(const uint64_t job_type, const uint64_t id,
                      std::chrono::nanoseconds exectime) {
  std::lock_guard<std::mutex> l(d_->lock);
  auto &estimator = d_->find(job_type);
  {
    using namespace std::chrono;
    const auto job = estimator.remove(id);
    estimator.llsp.add(job.metrics.data(),
                       duration_cast<duration<double>>(exectime).count());
    estimator.llsp.solve();
  }
}

bool estimator::operator==(const estimator &rhs) const {
  return *d_ == *rhs.d_;
}
}
