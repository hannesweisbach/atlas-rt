#include <iostream>

#include "runtime/dispatch.h"

static void f1() {}
static void f2(int) {}
static void f3(int &) {}
static void f4(const int &) {}
static void func2() { std::cout << "function pointer" << std::endl; }
static void func(int arg) {
  std::cout << "function pointer with arg " << arg << std::endl;
}

static void funcref(int &i) {
  std::cout << "function pointer with arg " << i++;
  std::cout << " " << i++ << std::endl;
}

int main() {
  using namespace std::chrono;
  using namespace std::literals::chrono_literals;
  atlas::dispatch_queue queue("test");

  int foo;
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), f1);
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), f2, 3);
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), f2, foo);
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), f2, std::ref(foo));
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), f3, std::ref(foo));
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), f4, 3);
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), f4, foo);
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), f4, std::ref(foo));
#if 1
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), [](int &) {}, std::ref(foo));
#endif

  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), std::bind(f2, 3));
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), std::bind(f2, foo));
  queue.sync(steady_clock::now(), static_cast<const double *>(nullptr),
             size_t(0), std::bind(f2, std::placeholders::_1), 4);

  std::cout << 1 << std::endl;
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), [] { std::cout << "lambda" << std::endl; });
  std::cout << 2 << std::endl;
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0),
              [](char) { std::cout << "variadic lambda" << std::endl; }, 'c');
  std::cout << 3 << std::endl;
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), func, 5);
  std::cout << 4 << std::endl;
  queue.async(steady_clock::now() + 1s, static_cast<const double *>(nullptr),
              size_t(0), func2);
  std::cout << 5 << std::endl;
  int i = 3;
  queue.sync(funcref, std::ref(i));
  std::cout << "after: " << i << std::endl;
}
