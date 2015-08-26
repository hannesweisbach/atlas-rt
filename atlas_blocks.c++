#include <iostream>

#include "dispatch/dispatch.h"

static void l2(atlas::dispatch_queue &queue) {
  queue.dispatch_sync_atlas(std::chrono::steady_clock::now(),
                            static_cast<double *>(nullptr), 0, ^{
                                                            });
};
static void l1(atlas::dispatch_queue &queue) { l2(queue); }

int main() {
  using namespace std::chrono;
  using namespace std::literals::chrono_literals;
  atlas::dispatch_queue queue("test");
  auto b = ^(int &) {
  };
  // b(4);
  int foo = 1;
  std::bind(^(int &){
            }, 4);
  std::bind([](int &) {}, 3);
  queue.dispatch_sync_atlas(steady_clock::now(), static_cast<double *>(nullptr),
                            0, ^{
                            });
  queue.dispatch_sync_atlas(steady_clock::now(), static_cast<double *>(nullptr),
                            0, ^{
                              static_cast<void>(foo);
                            });
  queue.dispatch_sync_atlas(steady_clock::now(), static_cast<double *>(nullptr),
                            0, ^(int){
                            }, 3);
  queue.dispatch_sync_atlas(steady_clock::now(), static_cast<double *>(nullptr),
                            0, ^(int &i) {
                              i = 4;
                            }, 3);
  queue.dispatch_sync_atlas(steady_clock::now(), static_cast<double *>(nullptr),
                            0, ^(const int &){
                            }, 3);
  l1(queue);
  l2(queue);

  auto block = ^{
    std::cout << "block" << std::endl;
  };
  queue.dispatch_async_atlas(steady_clock::now() + 1s,
                             static_cast<double *>(nullptr), 0, block);
  std::cout << 6 << std::endl;
  queue.dispatch_async_atlas(steady_clock::now() + 1s,
                             static_cast<double *>(nullptr), 0, block);
  std::cout << 7 << std::endl;
  queue.dispatch_async_atlas(steady_clock::now() + 1s,
                             static_cast<double *>(nullptr), 0, ^{
                               std::cout << "block2" << std::endl;
                             });
  std::cout << 8 << std::endl;
  queue.dispatch_async_atlas(steady_clock::now() + 1s,
                             static_cast<double *>(nullptr), 0, ^(int i) {
                               std::cout << "block with arg " << i << std::endl;
                             }, 43);
  std::cout << 9 << std::endl;
  queue.dispatch_async_atlas(steady_clock::now() + 1s,
                             static_cast<double *>(nullptr), 0, ^(int i) {
                               std::cout << "block with arg " << i << std::endl;
                             }, 44);
  std::cout << 10 << std::endl;
  queue.dispatch_async_atlas(steady_clock::now() + 1s,
                             static_cast<double *>(nullptr), 0, ^(int &i) {
                               std::cout << "block with arg " << i << std::endl;
                             }, 44);
  std::cout << 11 << std::endl;
  queue.dispatch_async_atlas(steady_clock::now() + 1s,
                             static_cast<double *>(nullptr), 0, ^(int &i) {
                               std::cout << "block with arg " << i << std::endl;
                             }, 44);
  queue.dispatch_sync(^{
    std::cout << "block" << std::endl;
  });

  queue.dispatch_async(^(int arg) {
    std::cout << "block with arg " << arg << std::endl;
  }, 6);

  int i = 0;
  queue.dispatch_sync(^(int &arg) {
    std::cout << "block with arg " << arg++ << std::endl;
  }, std::ref(i));
  std::cout << "after: " << i << std::endl;
}
