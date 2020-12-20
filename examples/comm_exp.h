#ifndef COMM_EXP_H_
#define COMM_EXP_H_

#include <sys/time.h>
#include <sched.h>
#include <stdio.h>

#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#define MIN_MSG (1)
#define MAX_MSG (4*1024*1024)

#define MYBUFSIZE 8192
#define MAX_MSG_SIZE MYBUFSIZE

#define LARGE 8192

#define TOTAL 4000
#define SKIP 1000

#define TOTAL_LARGE 1000
#define SKIP_LARGE 100

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef unlikely
#define unlikely(x) __builtin_expect((!!x),0)
#endif

#ifdef YP_USE_THREAD
#define TBARRIER() thread_barrier();
#else
#define TBARRIER() ;
#endif

static inline int comm_set_me_to(int core_id)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

static inline double wtime()
{
  struct timeval t1;
  gettimeofday(&t1, 0);
  return t1.tv_sec + t1.tv_usec / 1e6;
}

static inline double wutime()
{
  struct timeval t1;
  gettimeofday(&t1, 0);
  return t1.tv_sec * 1e6 + t1.tv_usec;
}

#ifndef MAX
#define MAX(a, b) ((a > b) ? (a) : (b))
#endif

static inline unsigned long long get_rdtsc()
{
  unsigned hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  unsigned long long cycle =
      ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
  return cycle;
}

static inline void busywait_cyc(unsigned long long delay)
{
  unsigned long long start_cycle, stop_cycle, start_plus_delay;
  start_cycle = get_rdtsc();
  start_plus_delay = start_cycle + delay;
  do {
    stop_cycle = get_rdtsc();
  } while (stop_cycle < start_plus_delay);
}

static inline double get_latency(double time, double n_msg)
{
  return time / n_msg;
}

static inline double get_msgrate(double time, double n_msg)
{
  return n_msg / time;
}

static inline double get_bw(double time, size_t size, double n_msg)
{
  return n_msg * size / time;
}

#ifdef __cplusplus
#include <mutex>
#include <condition_variable>
class cpp_barrier
{
 private:
  std::mutex _mutex;
  std::condition_variable _cv;
  std::size_t _count;
 public:
  explicit cpp_barrier(std::size_t count) : _count{count} { }
  void wait()
  {
    std::unique_lock<std::mutex> lock{_mutex};
    if (--_count == 0) {
      _cv.notify_all();
    } else {
      _cv.wait(lock, [this] { return _count == 0; });
    }
  }
};

namespace {
#ifdef USE_PAPI
#include <papi.h>
static int EV[1] = { PAPI_L3_TCM };

static inline void wreset() {
  // PAPI_thread_init(pthread_self);
  PAPI_start_counters(EV, 1);
}

static inline long long wcount(long long *state) {
  PAPI_accum_counters(state, 1);
  return *state;
}

#else
static inline void wreset() {}
static inline long long wcount(long long *state) {
  return 0;
}
#endif
}

#define _memalign posix_memalign
#define _free free

#include "bench_omp.hpp"
using namespace omp;

static inline void print_banner()
{
  fprintf(stdout, "%-10s %-10s %-10s %-10s %-10s\n", "Size", "us", "Mmsg/s", "MB/s", "events");
  fflush(stdout);
}

template<typename FUNC>
static inline void RUN_VARY_MSG(std::pair<size_t, size_t>&& range,
                         const int report,
                         FUNC&& f, std::pair<int, int>&& iter = {0, 1}, char* extra_str = NULL)
{
  double t = 0;
  int loop = TOTAL;
  int skip = SKIP;
  long long state;
  long long count = 0;


  wreset();

  for (size_t msg_size = range.first; msg_size <= range.second; msg_size <<= 1) {
    if (msg_size >= LARGE) {
      loop = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }

    for (int i = iter.first; i < skip; i+=iter.second) {
      f(msg_size, i);
    }

    TBARRIER();
    count = wcount(&state);
    t = wtime();

    for (int i = iter.first; i < loop; i += iter.second) {
      f(msg_size, i);
    }

    TBARRIER();
    t = wtime() - t;
    count = wcount(&state) - count;

    if (report && thread_id() == 0) {
      double latency = 1e6 * get_latency(t, 2.0 * loop);
      double msgrate = get_msgrate(t, 2.0 * loop) / 1e6;
      double bw = get_bw(t, msg_size, 2.0 * loop) / 1024 / 1024;
      double event = count / (2.0 * (loop / iter.second));
      if (extra_str)
        fprintf(stdout, "%-10lu %-10.2f %-10.3f %-10.2f %-10.2f %-10s\n",
                msg_size, latency, msgrate, bw, event, extra_str);
      else
        fprintf(stdout, "%-10lu %-10.2f %-10.3f %-10.2f %-10.2f\n",
                msg_size, latency, msgrate, bw, event);
      fflush(stdout);
    }
  }

  TBARRIER();
}

#endif
#endif
