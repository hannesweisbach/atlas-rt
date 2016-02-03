#include <iostream>
#include <chrono>
#include <cerrno>
#include <thread>
#include <atomic>
#include <ostream>

#include "atlas/atlas.h"
#include "type_list.h"
#include "test_cases.h"

static uint64_t id{0};

namespace atlas {
namespace test {
template <typename Tid, typename Exectime, typename Deadline>
struct submit_test {
  static result test(std::ostringstream &os) {
    Tid tid;
    auto err = atlas::submit(tid.tid(), ++id, Exectime::tv(), Deadline::tv());
    result test_result{errno, err != 0};

    os << "TID " << tid.tid() << ", exec time  " << Exectime::tv()
       << " and deadline " << Deadline::tv();
    return test_result;
  }
};
template <typename... Us> using submit = testcase<submit_test, Us...>;
}
}

int main() {
  using Tids =
      type_list<tid_thread, tid_self, tid_negative, tid_invalid, tid_init>;
  using Deadlines = Times;
  using Exectimes = Times;
  using combination = combinator<Tids, Exectimes, Deadlines>;

  using testsuite = apply<atlas::test::submit, typename combination::type>;

  testsuite::invoke();
}
