#include <chrono>
#include <cinttypes>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <linux/perf_event.h>
#include <jevents.h>
#include <rdpmc.h>
}

#include "cputime_clock.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"

namespace atlas { 
namespace pmu {

class Event {

protected:

  virtual long long int read_counter() = 0;

public:
  long long old = 0;
  long long value = 0;
  long long raw_value = 0;
  virtual ~Event() = default;
  void begin() {
    set_raw_value(0);
    end();
  }
  void end() { 
    const auto tmp = read_counter();
    value = tmp - old;
    old = tmp;
  }
  auto peek() { return read_counter() - old; }
  void set_raw_value(const long long raw) { raw_value = raw; }
  auto sample() const { return value; }
};

class ClockEvent : public Event {
  long long int read_counter() override {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }
};

class CPUTimeEvent : public Event {
  long long int read_counter() override {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               cputime_clock::now().time_since_epoch())
        .count();
  }
};

class PMUEvent : public Event {
  std::shared_ptr<rdpmc_ctx> context;

  long long int read_counter() override {
    return static_cast<long long>(rdpmc_read(context.get()));
  }

public:
  PMUEvent(std::string name)
      : context(new rdpmc_ctx, [](rdpmc_ctx *ctx) { rdpmc_close(ctx); }) {
    struct perf_event_attr attr;
    if (resolve_event(const_cast<char *>(name.c_str()), &attr)) {
      std::cerr << "Error resolving event " << name << std::endl;
      exit(EXIT_FAILURE);
    }

    if (rdpmc_open_attr(&attr, context.get(), nullptr)) {
      std::cerr << "Error opening counter " << name << std::endl;
      exit(EXIT_FAILURE);
    }
  }
};

class RawEvent : public Event {
  long long read_counter() override { return raw_value; }
};

class ROI {
  std::vector<std::shared_ptr<Event>> events;
  Event *id;
  Event *dl;

  Event *predictor_event;
  Event *exectime;
  Event *makespan;
  Event *tardiness;
  Event *predictor_error;
  Event *type;

  Event *tmp1;
  Event *tmp2;

public:
  ROI() {
    events.push_back(std::make_shared<RawEvent>());
    id = events.back().get();

    events.push_back(std::make_shared<RawEvent>());
    dl = events.back().get();

    events.push_back(std::make_shared<ClockEvent>());
    events.push_back(std::make_shared<CPUTimeEvent>());

    events.push_back(std::make_shared<RawEvent>());
    makespan = events.back().get();
    events.push_back(std::make_shared<RawEvent>());
    tardiness = events.back().get();

    events.push_back(std::make_shared<RawEvent>());
    predictor_event = events.back().get();
    events.push_back(std::make_shared<RawEvent>());
    predictor_error = events.back().get();
    
    //events.push_back(std::make_shared<PMUEvent>("mem_load_retired.llc_miss"));
    
    events.push_back(std::make_shared<RawEvent>());
    type = events.back().get();

    events.push_back(std::make_shared<RawEvent>());
    tmp1 = events.back().get();
    events.push_back(std::make_shared<RawEvent>());
    tmp2 = events.back().get();
  }

  void begin() {
    for (auto &&event : events) {
      event->begin();
    }
  }

  void sample(const atlas::work_item &work) {
    id->set_raw_value(reinterpret_cast<long long>(&work));
    const auto submit = work.submit.time_since_epoch();
    const auto deadline = work.deadline.time_since_epoch();
    const auto now = atlas::clock::now().time_since_epoch();
    dl->set_raw_value(deadline.count());
    makespan->set_raw_value((now - submit).count());
    tardiness->set_raw_value((now - deadline).count());

    const auto cputime = events.at(1)->peek();
    using namespace std::chrono;
    predictor_error->set_raw_value(
        cputime - duration_cast<nanoseconds>(work.prediction).count());
    predictor_event->set_raw_value(work.prediction.count());

    type->set_raw_value(static_cast<long long>(work.type));

    tmp1->set_raw_value(duration_cast<nanoseconds>(work.prediction).count());
    tmp2->set_raw_value(cputime);
    for (auto &&event : events) {
      event->end();
    }
  }

  void log(std::ofstream& log) const {
    for (const auto &event : events) {
      log << std::setw(10) << event->sample() << " ";
    }
  }
};

}
}

#pragma clang diagnostic pop
