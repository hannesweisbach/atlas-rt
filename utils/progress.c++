/*
 * Copyright (C) 2006-2013 Michael Roitzsch <mroi@os.inf.tu-dresden.de>
 * economic rights: Technische Universitaet Dresden (Germany)
 */
#include <chrono>
#include <iostream>
#include <iomanip>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>

#include "runtime/cputime_clock.h"

using namespace std::chrono;
const static steady_clock::time_point start_time = steady_clock::now();
static cputime_clock::time_point progress = cputime_clock::now();

static void handler(int signal) {
  switch (signal) {
  case SIGALRM: {
    const auto offset =
        duration_cast<duration<double>>(steady_clock::now() - start_time)
            .count();
    const auto now = cputime_clock::now();
    const auto delta = duration_cast<duration<double>>(now - progress).count();
    progress = now;
    std::cout << std::fixed << std::setprecision(3) << offset << " " << delta
              << std::endl;
  } break;
  case SIGTERM:
    exit(0);
  default:;
  }
}

int main(void) {
  struct sigaction action = {};
  action.sa_handler = handler;

  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGALRM, &action, NULL);

  struct itimerval timer = {};
  timer.it_interval.tv_sec = 1;
  timer.it_interval.tv_usec = 0;
  timer.it_value.tv_sec = 1;
  timer.it_value.tv_usec = 0;

  setitimer(ITIMER_REAL, &timer, NULL);

  while (1)
    ;
}
