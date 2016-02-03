#include <future>
#include <iostream>
#include <cerrno>

#include <pthread.h>

#include "atlas/atlas.h"
#include "type_list.h"
#include "test_cases.h"

struct std_thread {
  std::thread t;
  std::atomic_bool running{true};
  std::promise<pid_t> promise;

  std_thread()
      : t([this] {
          promise.set_value(gettid());
          while (running)
            std::this_thread::yield();
        }) {}
  ~std_thread() {
    running = false;
    t.join();
  }
  decltype(auto) tid() { return promise.get_future().get(); }
  decltype(auto) handle() { return t.get_id(); }
};

struct pthread_thread {
  pthread_t t;
  std::atomic_bool running{true};
  std::promise<pid_t> promise;
  pthread_thread() {
    auto err = pthread_create(&t, nullptr, [](void *arg) {
      auto this_ = static_cast<pthread_thread *>(arg);
      this_->promise.set_value(gettid());
      while (this_->running)
        pthread_yield();
      return static_cast<void *>(nullptr);
    }, this);
    if (err != 0) {
      std::ostringstream os;
      os << "Unable to create thread (" << errno << "): " << strerror(errno);
      throw std::runtime_error(os.str());
    }
  }
  ~pthread_thread() {
    running = false;
    pthread_join(t, nullptr);
  }
  decltype(auto) tid() { return promise.get_future().get(); }
  auto handle() { return t; }
};

struct std_self {
  static decltype(auto) tid() { return gettid(); }
  static decltype(auto) handle() { return std::this_thread::get_id(); }
};

struct pthread_self {
  static decltype(auto) tid() { return gettid(); }
  static decltype(auto) handle() { return ::pthread_self(); }
};

namespace atlas {
namespace np {
namespace test {
template <typename Tid> struct np_test {
  static void invoke() {
    Tid tid_;
    auto tid = tid_.tid();
    auto handle = tid_.handle();
    auto np_tid = atlas::np::from(handle);

    if (tid != np_tid)
      std::cerr << "TID (" << np_tid << ") does not match gettid() TID (" << tid
                << ")" << std::endl;
  }
};
}
}
}

int main() {
  using Handles =
      type_list<std_thread, pthread_thread, std_self, struct pthread_self>;
  using combination = combinator<Handles>;

  using testsuite = apply<atlas::np::test::np_test, typename combination::type>;

  testsuite::invoke();
}

