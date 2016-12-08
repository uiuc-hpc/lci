#ifndef PROFILER_H_
#define PROFILER_H_

#include "config.h"

#ifdef USE_PAPI

#include <papi.h>
#include <pthread.h>

static void profiler_init()
{
    if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT) {
        exit(EXIT_FAILURE);
    }
    if (PAPI_thread_init((unsigned long (*)(void))(
            (unsigned long (*)(void))(pthread_self))) != PAPI_OK) {
        exit(EXIT_FAILURE);
    }
}

class profiler
{
 public:
  inline profiler(std::initializer_list<int> args)
  {
    std::cerr << "[USE_PAPI] Profiling in: " << PAPI_thread_id() << std::endl;
    events_ = args;
    counters_.resize(events_.size(), 0);
  }

  inline void start() { PAPI_start_counters(&events_[0], events_.size()); }
  inline const std::vector<long long>& stop()
  {
    PAPI_stop_counters(&counters_[0], counters_.size());
    return counters_;
  }

  inline void read(long long* val) { PAPI_read_counters(val, events_.size()); }
  inline void accum(long long* val)
  {
    PAPI_accum_counters(val, events_.size());
  }

  inline void print()
  {
    for (auto& x : counters_) std::cout << x << " ";
    std::cout << std::endl;
  }

 private:
  std::vector<int> events_;
  std::vector<long long> counters_;
};

#endif // USE_PAPI

#endif
