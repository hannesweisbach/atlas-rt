#include <iostream>
#include <cerrno>

#include "atlas/atlas.h"
#include "type_list.h"
#include "test_cases.h"

namespace atlas {
namespace test {
template <typename Tid, typename Jid, typename Exec, typename Deadline>
struct update_test {
  static result test(std::ostringstream &os) {
    Tid tid;
    auto job_id = Jid::submit(tid.tid(), Tid::valid());
    auto err = atlas::update(tid.tid(), job_id, Exec::tv(), Deadline::tv());
    result test_result{errno, err != 0};

    os << "TID " << tid.tid() << " and JID " << job_id;
    return test_result;
  }
};

template <typename... Us> using update = testcase<update_test, Us...>;
}
}

int main() {
  using Jids = type_list<jid_valid, jid_invalid>;
  using Deadlines = Times;
  using Exectimes = Times;
  using combination = combinator<Tids, Jids, Exectimes, Deadlines>;

  using testsuite = apply<atlas::test::update, typename combination::type>;

  testsuite::invoke();
}

