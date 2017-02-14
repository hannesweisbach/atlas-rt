#include "atlas-clock.h"

extern "C" {

struct timespec atlas_now(void) {
  using namespace std::chrono;
  auto now = atlas::clock::now().time_since_epoch();
  struct timespec ts;
  ts.tv_sec = duration_cast<seconds>(now).count();
  ts.tv_nsec = duration_cast<nanoseconds>(now - seconds(ts.tv_sec)).count();
  return ts;
}

}
