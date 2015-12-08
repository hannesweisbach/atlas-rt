#include <atomic>

#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <iostream>

#include "common.h"
#include "atlas/atlas.h" // atlas::np::from

/* Subtract the ‘struct timeval’ values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */

struct timespec operator-(const struct timespec &lhs,
                                 const struct timespec &rhs) {
  struct timespec x = lhs;
  struct timespec y = rhs;
  struct timespec result;
  /* Perform the carry for the later subtraction by updating y. */
  if (x.tv_nsec < y.tv_nsec) {
    long nsec = (y.tv_nsec - x.tv_nsec) / 1000000000 + 1;
    y.tv_nsec -= 1000000000 * nsec;
    y.tv_sec += nsec;
  }
  if (x.tv_nsec - y.tv_nsec > 1000000000) {
    long nsec = (x.tv_nsec - y.tv_nsec) / 1000000000;
    y.tv_nsec += 1000000 * nsec;
    y.tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result.tv_sec = x.tv_sec - y.tv_sec;
  result.tv_nsec = x.tv_nsec - y.tv_nsec;

  /* Return 1 if result is negative. */
  return result;
}

pid_t gettid() { return static_cast<pid_t>(syscall(SYS_gettid)); }
pid_t invalid_tid() {
  pid_t nonexistent;
  std::fstream max_pid("/proc/sys/kernel/pid_max", std::ios::in);
  max_pid >> nonexistent;
  return ++nonexistent;
}

void set_affinity(std::initializer_list<unsigned> cpus, pid_t tid) {
  cpu_set_t cpu_set;
  CPU_ZERO(&cpu_set);
  for (const auto &cpu : cpus)
    CPU_SET(cpu, &cpu_set);

  check_zero(sched_setaffinity(tid, sizeof(cpu_set_t), &cpu_set),
             "Error setting affinity");
}

void set_affinity(unsigned cpu, pid_t tid) { set_affinity({cpu}, tid); }
void set_affinity(unsigned cpu, std::thread::id id) { set_affinity({cpu}, id); }
void set_affinity(unsigned cpu, const std::thread &t) {
  set_affinity({cpu}, t);
}
void set_affinity(std::initializer_list<unsigned> cpus, std::thread::id id) {
  set_affinity(cpus, atlas::np::from(id));
}
void set_affinity(std::initializer_list<unsigned> cpus, const std::thread &t) {
  set_affinity(cpus, atlas::np::from(t));
}

void set_signal_handler(int signal, signal_handler_t handler) {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
#if defined(__GNU_LIBRARY__) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
  act.sa_sigaction = handler;
#if defined(__GNU_LIBRARY__) && defined(__clang__)
#pragma clang diagnostic pop
#endif
  act.sa_flags = SA_SIGINFO;
  check_zero(sigaction(signal, &act, nullptr),
             "Error establishing signal handler");
}

void set_deadline_handler(signal_handler_t handler) {
  set_signal_handler(SIGXCPU, handler);
}

static thread_local std::atomic_bool deadline_passed{false};

static void deadline_handler(int, siginfo_t *, void *) {
  deadline_passed = true;
}

void ignore_deadlines() {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
#if defined(__GNU_LIBRARY__) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
  act.sa_handler = SIG_IGN;
#if defined(__GNU_LIBRARY__) && defined(__clang__)
#pragma clang diagnostic pop
#endif
  check_zero(sigaction(SIGXCPU, &act, nullptr),
             "Error establishing signal handler");
}

void wait_for_deadline() {
  set_deadline_handler(&deadline_handler);
  while (!deadline_passed)
    ;

  deadline_passed = false;
}

void record_deadline_misses() { set_deadline_handler(&deadline_handler); }
bool reset_deadline() { return deadline_passed.exchange(false); }

std::ostream &operator<<(std::ostream &os, const enum attribute &attr) {
  switch (attr) {
  case attribute::min_slack:
    return os << "min_slack";
  case attribute::preroll:
    return os << "advance_in_cfs";
  case attribute::job_stealing:
    return os << "idle_job_stealing";
  case attribute::overload_push:
    return os << "overload_push";
  case attribute::wakeup_balance:
    return os << "wakeup_balance";
  }
}

