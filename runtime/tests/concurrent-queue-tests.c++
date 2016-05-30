#include <algorithm>
#include <chrono>
#include <future>
#include <thread>
#include <type_traits>
#include <vector>

#include "gtest/gtest.h"
#include "runtime/dispatch.h"

using namespace std::chrono;

auto generate_affinity_vector(unsigned num) {
  std::vector<int> cpus;

  std::generate_n(std::back_inserter(cpus), num, [] {
    static int cpu = 0;
    return cpu++;
  });

  return cpus;
}

auto generate_affinity_mask(unsigned num) {
  cpu_set_t cpus;
  CPU_ZERO(&cpus);

  for (int cpu = 0; cpu < static_cast<int>(num); ++cpu) {
    CPU_SET(cpu, &cpus);
  }

  return cpus;
}

TEST(DispatchTest, HandlesCtorDtorUP) {
  const auto max = std::thread::hardware_concurrency();
  for (std::decay_t<decltype(max)> cpus{0}; cpus < max; ++cpus) {
    atlas::dispatch_queue queue{"main queue", {static_cast<int>(cpus)}};
    std::this_thread::sleep_for(1ms);
  }
}

TEST(DispatchTest, HandlesCtorDtorMP) {
  const auto max = std::thread::hardware_concurrency();
  for (std::decay_t<decltype(max)> num{0}; num < max; ++num) {
    auto mask = generate_affinity_mask(num);
    atlas::dispatch_queue queue("main queue", &mask);
    std::this_thread::sleep_for(1ms);
  }
}

#if 0
TEST(DispatchTest, HandlesCtorDtorMPTooMany) {
  auto mask = generate_affinity_mask(std::thread::hardware_concurrency() + 1);
  EXPECT_THROW(atlas::dispatch_queue queue("main queue", &mask),
               std::runtime_error);
}
#endif

TEST(DispatchTest, HandlesDispatchAsyncUP) {
  atlas::dispatch_queue queue{"main queue", {0}};
  auto promise = std::make_shared<std::promise<void>>();
  queue.dispatch_async([=] { promise->set_value(); });
  promise->get_future().get();
}

TEST(DispatchTest, HandlesDispatchAsyncMP) {
  auto mask = generate_affinity_mask(std::thread::hardware_concurrency());
  atlas::dispatch_queue queue{"main queue", &mask};
  auto promise = std::make_shared<std::promise<void>>();
  queue.dispatch_async([=] { promise->set_value(); });
  promise->get_future().get();
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
