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

#ifdef USE_THREAD
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

void write_buffer(char* buffer, int len, char input) {
  for (int i = 0; i < len; ++i) {
    buffer[i] = input;
  }
}

void check_buffer(const char* buffer, int len, char expect) {
  for (int i = 0; i < len; ++i) {
    if (buffer[i] != expect) {
      abort();
    }
  }
}

#ifdef __cplusplus
#include <mutex>
#include <condition_variable>
#include "bench_omp.hpp"
#include "bench_config.h"
using namespace omp;

#define _memalign posix_memalign
#define _free free

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
int papi_events[] = { PAPI_L1_TCM, PAPI_L2_TCM, PAPI_L3_TCM };
const int PAPI_NUM = sizeof(papi_events) / sizeof(papi_events[0]);
char papi_event_names[PAPI_NUM][10] = { "L1_TCM", "L2_TCM", "L3_TCM" };

#define PAPI_SAFECALL(x)                                                    \
  {                                                                         \
    int err = (x);                                                          \
    if (err != PAPI_OK) {                                                   \
      printf("err : %d/%s (%s:%d)\n", err, PAPI_strerror(err), __FILE__, __LINE__); \
      exit(err);                                                            \
    }                                                                       \
  }                                                                         \
  while (0)                                                                 \
    ;
#endif
}

static inline void yp_init() {
#ifdef USE_PAPI
  int retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    fprintf(stderr, "PAPI library init error!\n");
    exit(1);
  }
#ifdef USE_THREAD
  PAPI_SAFECALL(PAPI_thread_init(pthread_self));
#endif
#endif
}

inline void print_banner()
{
  char str[256];
  int used = 0;
  used += snprintf(str+used, 256-used, "%-10s %-10s %-10s %-10s", "Size", "us", "Mmsg/s", "MB/s");
#ifdef USE_PAPI
  for (int i = 0; i < PAPI_NUM; ++i) {
    used += snprintf(str+used, 256-used, " %-10s", papi_event_names[i]);
  }
#endif
  printf("%s\n", str);
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

#ifdef USE_PAPI
  int papi_eventSet = PAPI_NULL;
  long_long papi_values[PAPI_NUM];
  PAPI_SAFECALL(PAPI_create_eventset(&papi_eventSet));
  PAPI_SAFECALL(PAPI_add_events(papi_eventSet, papi_events, PAPI_NUM));
#endif

  for (size_t msg_size = range.first; msg_size <= range.second; msg_size <<= 1) {
    if (msg_size >= LARGE) {
      loop = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }

    for (int i = iter.first; i < skip; i += iter.second) {
      f(msg_size, i);
    }

    TBARRIER();
#ifdef USE_PAPI
    PAPI_SAFECALL(PAPI_start(papi_eventSet));
#endif
    t = wtime();

    for (int i = iter.first; i < loop; i += iter.second) {
      f(msg_size, i);
    }

    TBARRIER();
    t = wtime() - t;
#ifdef USE_PAPI
    PAPI_SAFECALL(PAPI_stop(papi_eventSet, papi_values));
#endif

    if (report && thread_id() == 0) {
      double latency = 1e6 * get_latency(t, 2.0 * loop);
      double msgrate = get_msgrate(t, 2.0 * loop) / 1e6;
      double bw = get_bw(t, msg_size, 2.0 * loop) / 1024 / 1024;

      char output_str[256];
      int used = 0;
      used += snprintf(output_str + used, 256, "%-10lu %-10.2f %-10.3f %-10.2f",
                       msg_size, latency, msgrate, bw);

#ifdef USE_PAPI
      for (int i = 0; i < PAPI_NUM; ++i) {
        double event = (double)papi_values[i] / (2.0 * (loop / iter.second));
        used += snprintf(output_str + used, 256 - used, " %-10.2f", event);
      }
#endif
      if (extra_str)
        used += snprintf(output_str + used, 256 - used, " %-10s", extra_str);
      printf("%s\n", output_str);
      fflush(stdout);
    }
  }

  TBARRIER();
}

#endif
#endif
