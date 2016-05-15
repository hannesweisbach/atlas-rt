#include <algorithm>
#include <chrono>
#include <random>

#include "gtest/gtest.h"
#include "predictor/predictor.h"

using namespace std::chrono;
static constexpr const char *const fname = "test.json";

TEST(SaveTest, HandlesEmptySave) {
  atlas::estimator estimator;
  estimator.save(fname);
}

TEST(SaveTest, HandlesSave) {
  atlas::estimator estimator;
  static constexpr uint64_t job_type = 0xdead;
  static constexpr uint64_t id = 0xbeef;
  double metric = 1.0;

  estimator.predict(job_type, id, &metric, 1);
  estimator.save(fname);
  estimator.train(job_type, id, 1s);
  estimator.save(fname);
}

#if 1
TEST(SaveTest, HandlesLoad) {
  static constexpr uint64_t job_type = 0xdead;
  static constexpr uint64_t id = 0xbeef;
  std::array<double, 10> metrics;
  std::mt19937_64 generator;
  std::uniform_real_distribution<> distribution;

  for (size_t jobs = 0; jobs < metrics.size(); ++jobs) {
    for (size_t i = 0; i < metrics.size(); ++i) {
      std::cout << i << std::endl;
      atlas::estimator a;
      for (size_t job = 0; job < jobs; ++job) {
        std::generate(
            std::begin(metrics), std::end(metrics),
            [&generator, &distribution] { return distribution(generator); });

        a.predict(job_type, id, metrics.data(), i);
        a.train(job_type, id, 1s);
      }

      /* save */
      a.save(fname);
      /* load */
      atlas::estimator b(fname);

      EXPECT_EQ(b, a);

      std::generate(
          std::begin(metrics), std::end(metrics),
          [&generator, &distribution] { return distribution(generator); });

      a.predict(job_type, id, metrics.data(), i);
      a.train(job_type, id, 1s);
      b.predict(job_type, id, metrics.data(), i);
      b.train(job_type, id, 1s);

      EXPECT_EQ(b, a);
    }
  }
}
#endif

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
