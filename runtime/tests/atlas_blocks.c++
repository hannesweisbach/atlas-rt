#include <iostream>

#include "gtest/gtest.h"
#include "runtime/dispatch.h"

using namespace std::chrono;
using namespace std::literals::chrono_literals;

TEST(BlocksTest, HandlesSync) {
  atlas::dispatch_queue queue("test");
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), ^{
             });
}

TEST(BlocksTest, HandlesSyncCapture) {
  atlas::dispatch_queue queue("test");
  int foo = 1;
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), ^{
               ASSERT_EQ(foo, 1);
             });
}

TEST(BlocksTest, HandlesSyncArgImm) {
  atlas::dispatch_queue queue("test");
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), ^(int i) {
               ASSERT_EQ(i, 3);
             }, 3);
}

TEST(BlocksTest, HandlesSyncArgRef) {
  atlas::dispatch_queue queue("test");
  int arg = 3;
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), ^(int &i) {
               ASSERT_EQ(i, 3);
               i = 4;
             }, std::ref(arg));
}

TEST(BlocksTest, HandlesSyncArgConstRefImm) {
  atlas::dispatch_queue queue("test");
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), ^(const int &i) {
               ASSERT_EQ(i, 3);
             }, 3);
}

TEST(BlocksTest, HandlesAsync) {
  atlas::dispatch_queue queue("test");
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), ^{
              });
}

TEST(BlocksTest, HandlesAsyncArgImm) {
  atlas::dispatch_queue queue("test");
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), ^(int i) {
                ASSERT_EQ(i, 43);
              }, 43);
}

TEST(BlocksTest, HandlesAsyncSame) {
  atlas::dispatch_queue queue("test");
  auto block = ^{
  };
  /* same type */
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), block);
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), block);
}

TEST(BlocksTest, HandlesAsyncArgImmDifferent) {
  atlas::dispatch_queue queue("test");
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), ^(int i) {
                ASSERT_EQ(i, 43);
              }, 43);
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), ^(int i) {
                ASSERT_EQ(i, 44);
              }, 44);
}

TEST(BlocksTest, HandlesAsyncArgRef) {
  atlas::dispatch_queue queue("test");
  int arg = 43;
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), ^(int &i) {
                ASSERT_EQ(i, 43);
              }, std::ref(arg));
}

TEST(BlocksTest, HandlesSyncNonRt) {
  atlas::dispatch_queue queue("test");
  queue.sync(^{
  });
}

TEST(BlocksTest, HandlesAsyncArgImmNonRt) {
  atlas::dispatch_queue queue("test");
  queue.async(^(int arg) {
    ASSERT_EQ(arg, 6);
  }, 6);
}

TEST(BlocksTest, HandlesAsyncArgRefNonRt) {
  atlas::dispatch_queue queue("test");
  int i = 0;
  queue.sync(^(int &arg) {
    ASSERT_EQ(arg, 0);
    ++arg;
  }, std::ref(i));
  ASSERT_EQ(i, 1);
}

static void l2(atlas::dispatch_queue &queue) {
  queue.sync(std::chrono::steady_clock::now(),
             static_cast<const double *>(nullptr), size_t(0), ^{
                                                   });
};

static void l1(atlas::dispatch_queue &queue) { l2(queue); }

TEST(BlocksTest, PassQueueTest) {
  atlas::dispatch_queue queue("test");
  l1(queue);
  l2(queue);
}
int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

#if 0
  // TODO: forbid submission of bind expressions
  std::bind(^(int &){
            }, 4);
  std::bind([](int &) {}, 3);
#endif

}
