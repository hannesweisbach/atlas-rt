#include "gtest/gtest.h"
#include "runtime/gcd-compat.h"

TEST(DispatchTest, HandlesCreation) {
  dispatch_queue_t queue = dispatch_queue_create("test", NULL);
  dispatch_queue_release(queue);
}

static void func(void *ctx) {
  int *i = static_cast<int *>(ctx);
  *i = 1;
}

TEST(DispatchTest, HandlesAsyncF) {
  dispatch_queue_t queue = dispatch_queue_create("test", NULL);
  int i = 0;
  dispatch_async_f(queue, &i, func);
  dispatch_queue_release(queue);
  ASSERT_EQ(i, 1);
}

TEST(DispatchTest, HandlesSyncF) {
  dispatch_queue_t queue = dispatch_queue_create("test", NULL);
  int i = 0;
  dispatch_sync_f(queue, &i, func);
  ASSERT_EQ(i, 1);
  dispatch_queue_release(queue);
}

TEST(DispatchTest, HandlesAsync) {
  dispatch_queue_t queue = dispatch_queue_create("test", NULL);
  int __block i = 0;
  dispatch_async(queue, ^{
    ++i;
  });
  dispatch_queue_release(queue);
  ASSERT_EQ(i, 1);
}

TEST(DispatchTest, HandlesSync) {
  dispatch_queue_t queue = dispatch_queue_create("test", NULL);
  int __block i = 0;
  dispatch_sync(queue, ^{
    ++i;
  });
  ASSERT_EQ(i, 1);
  dispatch_queue_release(queue);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
