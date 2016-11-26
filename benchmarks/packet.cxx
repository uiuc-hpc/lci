#define USE_AFFI
#define CONFIG_CONCUR 64
#include "comm_exp.h"
#include "mpiv.h"
#include <atomic>
#include <chrono>
#include <thread>

int NTHREADS = 4;
int PER_THREAD = 16;

extern __thread int wid;

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

template <class T>
void benchmarks()
{
  T pkg;
  pkg.init_worker(NTHREADS);
  for (int i = 0; i < MAX_CONCURRENCY; i++) {
    pkg.ret_packet(new mpiv_packet());
  }
  std::atomic<int> f;
  std::vector<double> times(TOTAL_LARGE - SKIP_LARGE, 0);

  srand(1234);
  std::vector<int> rands;
  std::thread th[NTHREADS];
  PER_THREAD = 256 / (MAX_CONCURRENCY / NTHREADS);
  for (int k = 0; k < TOTAL_LARGE; k++) {
    rands.clear();
    for (int i = 0; i < PER_THREAD * (NTHREADS); i++) {
      rands.push_back(rand() % (MAX_CONCURRENCY / NTHREADS) + 1);
    }
    cache_invalidate();
    f = 0;
    for (int i = 0; i < NTHREADS; i++) {
      th[i] = std::move(std::thread([&] {
        std::vector<mpiv_packet*> pp(MAX_CONCURRENCY);
        int ff = f.fetch_add(1);
        affinity::set_me_to_(ff);
        wid = ff;
        while (f < NTHREADS) {
        }
        double t1 = 0;
        if (ff == 0 && k >= SKIP_LARGE) t1 = wutime();
        int sumnrun = 0;
        for (int j = 0; j < PER_THREAD; j++) {
          int nruns = rands[ff * PER_THREAD + j];
          for (int jj = 0; jj < nruns; jj++) {
            pp[jj] = pkg.get_for_send();
          }
          if (k >= SKIP_LARGE && ff == 0) times[k - SKIP_LARGE] += wutime();
          std::this_thread::sleep_for(
              std::chrono::microseconds(rands[ff * PER_THREAD]));
          if (k >= SKIP_LARGE && ff == 0) times[k - SKIP_LARGE] -= wutime();

          for (int jj = 0; jj < nruns; jj++) {
            pkg.ret_packet_to(pp[jj], wid);
          }
          sumnrun += nruns;
        }
        if (ff == 0 && k >= SKIP_LARGE) {
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

template <class T>
void benchmarks_cp()
{
  T pkg;
  pkg.init_worker(NTHREADS);
  for (int i = 0; i < MAX_CONCURRENCY; i++) {
    pkg.ret_packet(new mpiv_packet());
  }
  std::atomic<int> f;
  std::vector<double> times(TOTAL_LARGE - SKIP_LARGE, 0);

  srand(1234);
  std::vector<int> rands;
  for (int i = 0; i < PER_THREAD * NTHREADS; i++) {
    rands.push_back(rand() % (MAX_CONCURRENCY / NTHREADS) + 1);
  }

  std::thread th[NTHREADS];
  for (int k = 0; k < TOTAL_LARGE; k++) {
    cache_invalidate();
    f = 0;
    for (int i = 0; i < NTHREADS; i++) {
      th[i] = std::move(std::thread([&] {
        std::vector<mpiv_packet*> pp(MAX_CONCURRENCY);
        int ff = f.fetch_add(1);
        wid = ff;
        affinity::set_me_to_(ff);
        while (f < NTHREADS) {
        }
        double t1 = 0;
        for (int j = 0; j < PER_THREAD; j++) {
          int nruns = rands[ff * PER_THREAD + j];
          for (int jj = 0; jj < nruns; jj++) {
            pp[jj] = pkg.get_for_send();
          }
          if (ff == 0 && k >= SKIP_LARGE) t1 = wutime();
          for (int jj = 0; jj < nruns; jj++) {
            memset(pp[jj], 'A', 4096);
          }
          if (ff == 0 && k >= SKIP_LARGE)
            times[k - SKIP_LARGE] += (wutime() - t1) / nruns;
          for (int jj = 0; jj < nruns; jj++) {
            pkg.ret_packet_to(pp[jj], wid);
          }
        }
      }));
    }
    for (auto& t : th) {
      t.join();
    }
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

  for (auto& q : qu) q = q / PER_THREAD;

  printf("Time (get+return): %d %.3f %.3f %.3f %.3f %.3f\n", NTHREADS, qu[0],
         qu[1], qu[2], qu[3], qu[4]);
}

int main(int argc, char** args)
{
  if (argc > 1) NTHREADS = atoi(args[1]);
  cache_size = (8 * 1024 * 1024 / sizeof(int));
  cache_buf = (int*)malloc(sizeof(int) * cache_size);

  // benchmarks_cp<packet_manager_NUMA_STEAL>();
  // benchmarks_cp<packet_manager_LFSTACK>();
  // benchmarks_cp<packet_manager_MPMCQ>();
  benchmarks<packet_manager_NUMA_STEAL>();
  benchmarks<packet_manager_LFSTACK>();
  benchmarks<packet_manager_MPMCQ>();
  return 0;
}

void main_task(intptr_t) {}
