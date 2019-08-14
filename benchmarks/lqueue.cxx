#include "comm_exp.h"
#include "mv.h"
#include "config.h"
#include "lcrq.h"
#include "dequeue.h"

__thread int mv_core_id = -1;

#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>
#include <vector>

int NTHREADS = 4;
int PER_THREAD = 16;

int* cache_buf;
int cache_size;

static void cache_invalidate(void)
{
  int i;
  cache_buf[0] = 1;
  for (i = 1; i < cache_size; ++i) {
    cache_buf[i] = cache_buf[i - 1];
  }
}

#if 1
#define QUEUE_T lcrq_t
#define QUEUE_INIT(q) lcrq_init(q)
#define QUEUE_EN(q, i) lcrq_enqueue(q, i)
#define QUEUE_DE(q) lcrq_dequeue(q)
#else

#define QUEUE_T dequeue
#define QUEUE_INIT(q) dq_init(q)
#define QUEUE_EN(q, i) dq_push_top(q, i)
#define QUEUE_DE(q) dq_pop_bot(q)
#endif

void benchmarks()
{
  QUEUE_T q;
  QUEUE_INIT(&q);

  std::atomic<int> f;
  std::vector<double> times(TOTAL_LARGE - SKIP_LARGE, 0);

  srand(1234);
  std::vector<int> rands;
  std::thread th[NTHREADS];
  PER_THREAD = MAX_PACKET / (MAX_PACKET / NTHREADS);
  double l1 = 0;

  for (int k = 0; k < TOTAL_LARGE; k++) {
    rands.clear();
    int total_runs = 0;
    for (int i = 0; i < PER_THREAD * (NTHREADS); i++) {
      rands.push_back(rand() % (MAX_PACKET / NTHREADS) + 1);
      total_runs += rands.back();
    }
    cache_invalidate();
    f = 0;
    for (int i = 0; i < NTHREADS; i++) {
      th[i] = std::move(std::thread([&, i] {
        int ff = f.fetch_add(1);
        // set_me_to_(ff);
        while (f < NTHREADS) {
        }
        double t1 = 0;
        if (i == 1 && k >= SKIP_LARGE) {
          t1 = wutime();
        }

        if (i == 0) {
          for (int j = 0; j < total_runs; j++) {
            QUEUE_DE(&q);
            busywait_cyc(1000);
          }
        }

        int sumnrun = 0;
        for (int j = 0; j < PER_THREAD; j++) {
          int nruns = rands[i * PER_THREAD + j];
          for (int jj = 0; jj < nruns; jj++) {
            QUEUE_EN(&q, (void*)jj);
          }
          sumnrun += nruns;
#if 0
          if (k >= SKIP_LARGE && i == 1) times[k - SKIP_LARGE] += wutime();
          std::this_thread::sleep_for(
              std::chrono::microseconds(rands[i * PER_THREAD]));
          if (k >= SKIP_LARGE && i == 1) times[k - SKIP_LARGE] -= wutime();
#endif
        }

        if (i == 1 && k >= SKIP_LARGE) {
          // printf("%d %.5f\n", PER_THREAD, times[k - SKIP_LARGE]);
          times[k - SKIP_LARGE] += ((wutime() - t1));
          times[k - SKIP_LARGE] /= sumnrun;
        }
      }));
    }
    for (int i = 0; i < NTHREADS; i++) th[i].join();
  }

  int size = times.size();
  std::sort(times.begin(), times.end());
  std::vector<double> qu(5, 0.0);
  qu[0] = (size % 2 == 1)
              ? (times[size / 2])
              : ((times[size / 2 - 1] + times[size / 2]) / 2);  // median
  qu[1] = times[size * 3 / 4];                                  // u q
  qu[2] = times[size / 4];                                      // d q
  double iq = qu[1] - qu[2];
  qu[3] = std::min(qu[1] + 1.5 * iq, times[size - 1]);  // max
  qu[4] = std::max(qu[2] - 1.5 * iq, times[0]);         // min

  // for (auto &q : qu) q = q / PER_THREAD;

  printf("Time (get+return): %d %.3f %.3f %.3f %.3f %.3f\n", NTHREADS, qu[0],
         qu[1], qu[2], qu[3], qu[4]);
}

int main(int argc, char** args)
{
  if (argc > 1) NTHREADS = atoi(args[1]);
  cache_size = (8 * 1024 * 1024 / sizeof(int));
  cache_buf = (int*)malloc(sizeof(int) * cache_size);
  benchmarks();
  return 0;
}

void main_task(intptr_t) {}
