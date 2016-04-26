#include <future>

#include <cerrno>

#include <boost/program_options.hpp>

#include "atlas/atlas.h"
#include "type_list.h"
#include "test_cases.h"

class TPId_valid {
  uint64_t id;

public:
  TPId_valid() = default;
  ~TPId_valid() { atlas::threadpool::destroy(id); }
  TPId_valid(const TPId_valid &) = delete;
  TPId_valid &operator=(const TPId_valid &) = delete;
  uint64_t *ptr() { return &id; }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

class TPId_nullptr {
public:
  uint64_t *ptr() { return static_cast<uint64_t *>(nullptr); }
  static void result(result &result) {
    if (result.error && result.error_code == EINVAL)
      result.accept = true;
  }
};

class TPId_invalid {
public:
  uint64_t *ptr() { return reinterpret_cast<uint64_t *>(0xdeadbabe); }
  static void result(result &result) {
    if (result.error && result.error_code == EFAULT)
      result.accept = true;
  }
};

struct TP_invalid {
  static uint64_t id() { return 0; }
  static void result(result &result) {
    if (result.error && result.error_code == EINVAL)
      result.accept = true;
  }
};

struct TP_invalid_thread {
  uint64_t id_;
  std::thread worker;
  std::mutex block;
  TP_invalid_thread() {
    id_ = atlas::threadpool::create();
    block.lock();
    std::promise<void> init;
    auto future = init.get_future();
    worker = std::thread([ this, init = std::move(init) ]() mutable {
      set_affinity(0);
      atlas::threadpool::join(id_);
      init.set_value();
      block.lock();
    });
    future.get();
  }
  ~TP_invalid_thread() {
    block.unlock();
    worker.join();
    atlas::threadpool::destroy(id_);
  }
  uint64_t id() { return id_; }
  static void result(result &result) {
    if (result.error && result.error_code == EBUSY)
      result.accept = true;
  }
};

struct TP_valid {
  uint64_t id_;
  TP_valid() : id_(atlas::threadpool::create()) {}
  TP_valid(const TP_valid &) = delete;
  TP_valid &operator=(const TP_valid &) = delete;
  uint64_t id() { return id_; }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

struct TP_empty {
  uint64_t id_;
  TP_empty() : id_(atlas::threadpool::create()) {}
  TP_empty(const TP_empty &) = delete;
  TP_empty &operator=(const TP_empty &) = delete;
  uint64_t id() { return id_; }
  static void result(result &result) {
    if (result.error && result.error_code == EBUSY)
      result.accept = true;
  }
};

struct TP_thread {
  uint64_t id_;
  std::thread worker;
  std::mutex block;
  TP_thread() : id_(atlas::threadpool::create()) {
    block.lock();
    std::promise<void> init;
    auto future = init.get_future();
    worker = std::thread([ this, init = std::move(init) ]() mutable {
      set_affinity(0);
      atlas::threadpool::join(id_);
      init.set_value();
      block.lock();
    });
    future.get();
  }
  ~TP_thread() {
    block.unlock();
    worker.join();
    atlas::threadpool::destroy(id_);
  }
  uint64_t id() { return id_; }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

struct Worker_valid {
  static void setup() { set_affinity(0); }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

struct Worker_invalid {
  static void setup() {}
  static void result(result &result) {
    if (result.error && result.error_code == EBUSY)
      result.accept = true;
  }
};

namespace atlas {
namespace test {
  template <typename TPId> struct create_test {
    static result test(std::ostringstream &os) {
      TPId id;
      auto err = atlas_tp_create(id.ptr());
      result test_result{errno, err != 0};

      os << "TPID ptr: " << id.ptr();
      return test_result;
    }
  };

  template <typename ThreadPool> struct destroy_test {
    static result test(std::ostringstream &os) {
      ThreadPool tp;
      auto tp_id = tp.id();
      auto err = atlas::threadpool::destroy(tp_id);
      result test_result{errno, err != 0};

      os << "TPID: " << std::hex << tp_id;
      return test_result;
    }
  };

  template <typename ThreadPool, typename Worker> struct join_test {
    static result test(std::ostringstream &os) {
      ThreadPool tp;
      auto tp_id = tp.id();
      std::promise<struct result> promise;
      auto future = promise.get_future();
      std::thread worker([ tp_id, promise = std::move(promise) ]() mutable {
        Worker::setup();
        auto err = atlas::threadpool::join(tp_id);
        promise.set_value({errno, err != 0});
      });
      worker.join();

      os << "TPID: " << std::hex << tp_id;
      return future.get();
    }
  };

  template <typename TP, typename Exectime, typename Deadline>
  struct submit_test {
    static result test(std::ostringstream &os) {
      static uint64_t id{0};
      TP tp;
      auto err = atlas::threadpool::submit(tp.id(), ++id, Exectime::tv(),
                                           Deadline::tv());
      result test_result{errno, err != 0};

      os << "TP " << std::hex << tp.id() << ", exec time  " << Exectime::tv()
         << " and deadline " << Deadline::tv();
      return test_result;
    }
  };

  template <typename... Us> using create = testcase<create_test, Us...>;
  template <typename... Us> using destroy = testcase<destroy_test, Us...>;
  template <typename... Us> using join = testcase<join_test, Us...>;
  template <typename... Us> using submit = testcase<submit_test, Us...>;
}
}

static void create() {
  using TPIds = type_list<TPId_nullptr, TPId_valid, TPId_invalid>;
  using combination = combinator<TPIds>;
  using testsuite = apply<atlas::test::create, typename combination::type>;

  testsuite::invoke();
}

static void destroy() {
  using TPs = type_list<TP_valid, TP_invalid, TP_invalid_thread>;
  using combination = combinator<TPs>;
  using testsuite = apply<atlas::test::destroy, typename combination::type>;

  testsuite::invoke();
}

static void join() {
  using TPs = type_list<TP_valid, TP_invalid, TP_invalid_thread>;
  using Workers = type_list<Worker_valid, Worker_invalid>;
  using combination = combinator<TPs, Workers>;
  using testsuite = apply<atlas::test::join, typename combination::type>;

  testsuite::invoke();
}

static void submit() {
  using TPs = type_list<TP_empty, TP_invalid, TP_thread>;
  using Deadlines = Times;
  using Exectimes = Times;
  using combination = combinator<TPs, Exectimes, Deadlines>;
  using testsuite = apply<atlas::test::submit, typename combination::type>;

  testsuite::invoke();
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("ATLAS thread pool interface testsuite");
  // clang-format off
  desc.add_options()
    ("help", "display help message")
    ("create", "Test atlas_threadpool_create syscall.")
    ("destroy", "Test atlas_threadpool_destroy syscall.")
    ("join", "Test atlas_threadpool_join syscall.")
    ("submit", "Test atlas_threadpool_submit syscall.");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("create") || vm.empty()) {
    create();
  }

  if (vm.count("destroy") || vm.empty()) {
    destroy();
  }

  if (vm.count("join") || vm.empty()) {
    join();
  }

  if (vm.count("submit") || vm.empty()) {
    submit();
  }
}
