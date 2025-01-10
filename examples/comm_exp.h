#ifndef COMM_EXP_H_
#define COMM_EXP_H_

#include <sys/time.h>
#include <sched.h>
#include <stdio.h>

#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#define MIN_MSG (1)
#define MAX_MSG (4 * 1024 * 1024)

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
#define unlikely(x) __builtin_expect((!!x), 0)
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

#define max(a, b) ((a > b) ? (a) : (b))

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
  explicit cpp_barrier(std::size_t count) : _count{count} {}
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
#endif
#endif
