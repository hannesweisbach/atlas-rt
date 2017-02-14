#pragma once

#include <iostream>

struct result {
  int64_t jobs{0};
  int64_t missed{0};

  result() = default;
  result(int64_t jobs_, int64_t missed_) : jobs(jobs_), missed(missed_) {}

  result operator+(const result &rhs) {
    return result{jobs + rhs.jobs, missed + rhs.missed};
  }

  result &operator+=(const result &rhs) {
    jobs += rhs.jobs;
    missed += rhs.missed;
    return *this;
  }

  friend std::ostream &operator<<(std::ostream &os, const result &rhs) {
    const auto ratio = double(rhs.missed) / rhs.jobs;
    const auto meta = (rhs.missed == 0) ? 0 : (ratio < 0.01) ? 500 : 1000;
    return os << ratio * 100 << " " << rhs.missed << " " << rhs.jobs << " "
              << meta;
  }
};


