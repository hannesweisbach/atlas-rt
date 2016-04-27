#include <functional>
#include <chrono>
#include <thread>
#include <atomic>

#include "runtime/dispatch.h"

static std::atomic_bool running{true};

struct executor {
  void runloop() {}
};

static void eventloop() {
  using namespace std::chrono;
  using namespace std::literals::chrono_literals;
  atlas::dispatch_queue queue("test");

  executor main;
  double metric = 100.0;

  for (; running;) {
    queue.dispatch_sync_atlas(steady_clock::now() + 1s, &metric, 1,
                              std::bind(&executor::runloop, &main));
  }
}

int main() {
  std::thread event(eventloop);

  std::cout << "Press Enter to quit." << std::endl;
  std::cin.ignore();
  std::cout << "Shutting down." << std::endl;
  running = false;
  if (event.joinable())
    event.join();
}
