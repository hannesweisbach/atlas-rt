#pragma once

#ifdef __cplusplus

#include <chrono>
#include <ctime>

namespace atlas {
using clock = std::chrono::steady_clock;
using time_point = clock::time_point;
}

#else
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct timespec atlas_now();

#ifdef __cplusplus
}
#endif
