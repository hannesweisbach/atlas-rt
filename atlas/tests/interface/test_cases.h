#pragma once

#include <thread>
#include <atomic>
#include <iostream>
#include <typeinfo>
#include <chrono>

#include "common/common.h"

namespace _ {
static uint64_t id{0};
}

struct result {
  int error_code;
  const bool error;
  bool accept;
  result(int error_code_, bool error_)
      : error_code(error_code_), error(error_), accept(false) {}
  template <typename Char>
  friend std::basic_ostream<Char> &operator<<(std::basic_ostream<Char> &os,
                                              const result &result) {
    os << (result.error ? "failed" : "succeeded");
    os << (result.accept ? " " : " un") << "expectedly";
    return os;
  }
};

template <template <typename...> class Test, typename... Ts>
struct testcase {
  template <typename T, typename... Ts_> struct type_printer {
    template <typename Char>
    friend std::basic_ostream<Char> &
    operator<<(std::basic_ostream<Char> &os, const type_printer<T, Ts_...> &) {
      return os << typeid(T).name() << " " << type_printer<Ts_...>{};
    }
  };

  template <typename T> struct type_printer<T> {
    template <typename Char>
    friend std::basic_ostream<Char> &operator<<(std::basic_ostream<Char> &os,
                                                const type_printer<T> &) {
      return os << typeid(T).name();
    }
  };

  template <typename U, typename... Us> struct result_checker {
    static void check(result &result) {
      U::result(result);
      result_checker<Us...>::check(result);
    }
  };

  template <typename U> struct result_checker<U> {
    static void check(result &result) { U::result(result); }
  };

  static void print_result(result &result, const std::string &arguments) {
    if (result.accept) {
      std::cout << "[PASS] ";
    } else {
      std::cout << "[FAIL] ";
    }

    std::cout << "Test " << type_printer<Ts...>{} << " " << result;
    if (result.error)
      std::cout << " with \"" << strerror(result.error_code) << "\" ("
                << result.error_code << ")";
    if (!arguments.empty())
      std::cout << " when invoked with " << arguments << std::endl;
  }
  static void invoke() {
    std::ostringstream arguments;
    std::cout << "Next Test is: " << type_printer<Ts...>{}
              << ". Press Enter to start." << std::endl;
    std::cin.ignore();
    auto result = Test<Ts...>::test(arguments);
    result_checker<Ts...>::check(result);
    print_result(result, arguments.str());
  }
};

struct tid_self {
  static decltype(auto) tid() { return gettid(); }
  static auto valid() { return true; }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

struct tid_thread {
  std::thread t;
  std::atomic_bool running{true};
  tid_thread()
      : t([this] {
          using namespace std::chrono;
          while (running)
            std::this_thread::sleep_for(10ms);
        }) {}
  ~tid_thread() {
    running = false;
    t.join();
  }
  decltype(auto) tid() { return atlas::np::from(t.get_id()); }
  static auto valid() { return true; }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

struct tid_negative {
  static auto tid() { return pid_t{-1}; }
  static auto valid() { return false; }
  static void result(result &result) {
    if (result.error && result.error_code == ESRCH)
      result.accept = true;
  }
};

struct tid_invalid {
  static decltype(auto) tid() { return invalid_tid(); }
  static auto valid() { return false; }
  static void result(result &result) {
    if (result.error && result.error_code == ESRCH)
      result.accept = true;
  }
};

struct tid_init {
  static auto tid() { return pid_t{1}; }
  static auto valid() { return false; }
  static void result(result &result) {
    if (result.error && result.error_code == EPERM)
      result.accept = true;
  }
};

struct jid_valid {
  static auto submit(const pid_t pid, const bool valid_pid) {
    using namespace std::chrono;

    ++_::id;
    if (!valid_pid)
      return _::id;

    atlas::submit(pid, _::id, 1s, high_resolution_clock::now() + 1s);
    return _::id;
  }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

struct jid_invalid {
  static auto submit(const pid_t, const bool) { return ++_::id; }
  static void result(result &result) {
    if (result.error && result.error_code == EINVAL)
      result.accept = true;
  }
};

struct tv_nullptr {
  static auto tv() { return static_cast<struct timeval *>(nullptr); }
  static void result(result &result) {
    if (result.error && result.error_code == EFAULT)
      result.accept = true;
  }
};

struct tv_1s {
  static auto tv() {
    static struct timeval tv { 1, 0 };
    return &tv;
  }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

using Tids =
    type_list<tid_thread, tid_self, tid_negative, tid_invalid, tid_init>;
using Times = type_list<tv_nullptr, tv_1s>;

