#include <iostream>
#include <cerrno>

#include "atlas/atlas.h"
#include "type_list.h"
#include "test_cases.h"

namespace atlas {
namespace test {
template <typename Tid, typename Jid> struct remove_test {
  static result test(std::ostringstream &os) {
    Tid tid;
    auto job_id = Jid::submit(tid.tid(), Tid::valid());
    auto err = atlas::remove(tid.tid(), job_id);
    result test_result{errno, err != 0};

    os << "TID " << tid.tid() << " and JID " << job_id;
    return test_result;
  }
};

template <typename... Us> using remove = testcase<remove_test, Us...>;
}
}

int main() {
  using Jids = type_list<jid_valid, jid_invalid>;

  using combination = combinator<Tids, Jids>;

  using testsuite = apply<atlas::test::remove, typename combination::type>;

  testsuite::invoke();
}
