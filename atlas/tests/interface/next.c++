#include <thread>
#include <iostream>
#include <chrono>
#include <atomic>
#include <future>

#include <cerrno>

#include <boost/program_options.hpp>

#include "atlas/atlas.h"
#include "type_list.h"
#include "test_cases.h"

using namespace std::chrono;

static uint64_t id{1};

/* Test the next syscall.
 * This tests correct wakeup & scheduling when sleeping/blocking in next().
 * Wakups are caused by:
 * - Explicit signals
 * - SIGKILL when the process exits (implicit)
 * - When a new job arrives
 *
 * The correct behaviour needs to be tested, regardless of the current
 * scheduler of the blocking task (there was a regression).
 *
 * 1) When blocking in ATLAS and a signal was delivered, there was an infinite
 *    scheduler loop, because ATLAS there was no job for ATLAS to schedule.
 * 2) When blocking in Recover a new job submission was added to the ATLAS
 *    RB-tree and the job was woken up. When Recover scheduled, it had no job,
 *    resulting in a BUG() or infinite scheduling loop.
 */

static void atlas_load() {}

/* workload to ensure blocking in Recover */
static void recover_load() {
  /* block to ensure Recover as scheduler */
  std::this_thread::sleep_for(0.5s);
  /* On deadline miss, transfer to Recover because of blocking time */
  wait_for_deadline();
}

static void cfs_load() {
  /* go to CFS */
  wait_for_deadline();
  busy_for(100ms);
}

static void test_submit(std::thread::id worker) {
  auto now = high_resolution_clock::now();
  atlas::np::submit(worker, id++, 1s, now + 2s);
}

static void test_signal(std::thread::id worker, int sig) {
  std::stringstream ss;
  ss << worker;

  pthread_t tid;
  ss >> tid;
  pthread_kill(tid, sig);
}

template <typename Workload, typename Test>
static bool wakeup(Workload &&w, Test &&t) {
  std::atomic_bool sleeping{false};
  std::atomic_bool done{false};
  using namespace std::chrono;

  std::thread worker([ w = std::move(w), &sleeping, &done ] {
    atlas::next();
    w();
    sleeping = true;
    atlas::next();
    done = true;
  });

  auto now = steady_clock::now();
  atlas::np::submit(worker, id++, 1s, now + 2s);

  /* Wait for the worker to block in the second next() call */
  std::this_thread::sleep_for(2s);
  while (!sleeping)
    ;
  std::this_thread::sleep_for(100ms);

  t(worker.get_id());

  std::this_thread::sleep_for(2.5s);
  if (done) {
    worker.join();
  } else {
    worker.detach();
  }

  return done;
}

/* TODO: to test process exit when a thread blocks in next, a new child process
 * needs to be spawned. The signal test below subsumes the exit test, since
 * sleeping threads are woken up by delivering SIGKILL.
 */
static void sighandler(int sig, siginfo_t *, void *) {
  std::cout << "Handling signal " << sig << std::endl;
}

static void restarting() {
  using namespace std::literals::chrono_literals;
  using namespace std::chrono;
  std::atomic_bool done{false};
  std::thread worker([&done] {
    set_signal_handler(SIGUSR1, sighandler);
    for (; !done;) {
      std::cout << "In next." << std::endl;
      atlas::next();
      std::cout << "Out next." << std::endl;
    }
  });

  atlas::np::submit(worker, id++, 1s, steady_clock::now() + 2s);
  std::this_thread::sleep_for(100ms);

  for (int i = 0; i < 10; ++i) {
    test_signal(worker.get_id(), SIGUSR1);
    std::this_thread::sleep_for(10ms);
  }

  done = true;
  atlas::np::submit(worker, id++, 1s, steady_clock::now() + 2s);
  worker.join();
}

struct nullptr_id {
  static uint64_t *id() { return nullptr; }
  static void result(result &result) {
    if (result.error && result.error_code == EFAULT)
      result.accept = true;
  }
};

struct invalid_id {
  static uint64_t *id() { return reinterpret_cast<uint64_t *>(0xdeadbabe); }
  static void result(result &result) {
    if (result.error && result.error_code == EFAULT)
      result.accept = true;
  }
};

struct valid_id {
  static uint64_t *id() {
    static uint64_t id_;
    return &id_;
  }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

namespace atlas {
namespace next {
  template <typename IdPtr> struct next_test {
    static result test(std::ostringstream &os) {
      std::promise<struct result> promise;
      auto future = promise.get_future();
      std::thread worker([promise = std::move(promise)]() mutable {
        auto err = atlas_next(IdPtr::id());
        promise.set_value({errno, err != 0});
      });

      atlas::np::submit(worker, 1, 1s, 1s);
      os << "IdPtr: " << std::hex << IdPtr::id();

      worker.join();
      return future.get();
    }
  };

  template <typename... Us> using next = testcase<next_test, Us...>;
}
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("Interface tests for atlas::submit()");
  // clang-format off
  desc.add_options()
    ("help", "produce help message")
    ("wakeup-atlas", "Submit job when blocking in next under ATLAS.")
    ("wakeup-recover", "Submit job when blocking in next under Recover.")
    ("wakeup-cfs", "Submit job when blocking in next under CFS.")
    ("signal-atlas", "Send signal to thread blocked in next under ATLAS.")
    ("signal-recover", "Send signal to thread blocked in next under Recover.")
    ("signal-cfs", "Send signal to thread blocked in next under CFS.")
    ("signal-repeat", "Test restarting of next() when blocking.")
    ("interface", "Run testsuite to check kernel interface.")
    ("all", "Run all test.");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }

  if (vm.count("wakeup-atlas") || vm.count("all"))
    wakeup(atlas_load, test_submit);
  if (vm.count("wakeup-recover") || vm.count("all"))
    wakeup(recover_load, test_submit);
  if (vm.count("wakeup-cfs") || vm.count("all"))
    wakeup(cfs_load, test_submit);

  auto test_sig = [](auto &&w) { test_signal(w, SIGCONT); };
  if (vm.count("signal-atlas") || vm.count("all"))
    wakeup(atlas_load, test_sig);
  if (vm.count("signal-recover") || vm.count("all"))
    wakeup(recover_load, test_sig);
  if (vm.count("signal-cfs") || vm.count("all"))
    wakeup(cfs_load, test_sig);

  if (vm.count("signal-repeat") || vm.count("all"))
    restarting();

  if (vm.count("interface") || vm.count("all")) {
    using IdPtrs = type_list<nullptr_id, valid_id, invalid_id>;
    using combination = combinator<IdPtrs>;
    using testsuite = apply<atlas::test::next, typename combination::type>;

    testsuite::invoke();
  }
}

