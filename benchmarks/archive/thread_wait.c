#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "lc/macro.h"

#include "comm_exp.h"

// #define USE_L1_MASK

#ifdef USE_ABT
#include "ult/helper_abt.h"
#elif defined(USE_PTH)
#include "ult/helper_pth.h"
#else
#include "ult/helper.h"
#endif

volatile int total = 0;
int total_threads;
int num_threads;
int num_worker;

void* f1(void* i)
{
  set_me_to((int)i);
  __sync_fetch_and_add(&total, 1);
  lc_sync* sync = lc_get_sync();
  thread_wait(sync);
  return 0;
}

int main(int argc, char** args)
{
#ifdef USE_ABT
  ABT_init(argc, args);
#endif
  if (argc > 1) num_threads = atoi(args[1]);
  if (argc > 1) num_worker = atoi(args[2]);

  printf("Num worker: %d, Num threads: %d\n", num_worker + 1, num_threads);

  total_threads = num_threads * num_worker;
  MPIV_Start_worker(num_worker + 1);

  lc_thread* tid = (lc_thread*)malloc(total_threads * sizeof(lc_thread));
  set_me_to(0);

  double t = 0;
  int loop = 100;
  double times[loop];

  for (int tt = 0; tt < loop; tt++) {
    total = 0;
    for (int i = 0; i < total_threads; i++) {
      tid[i] = MPIV_spawn(1 + i % num_worker, f1, (void*)(1 + i % num_worker));
    }
    while (total != total_threads) {
      thread_yield();
    };
    // usleep(10);
    times[tt] = wutime();
    for (int i = 0; i < total_threads; i++) {
      lc_sync* sync = (lc_sync*)tid[i];
      thread_signal(sync);
    }
    times[tt] = (wutime() - times[tt]) / total_threads;
    for (int i = 0; i < total_threads; i++) {
      MPIV_join(tid[i]);
    }
  }
  free(tid);

  double sum = 0;
  for (int i = 0; i < loop; i++) sum += times[i];
  double mean = sum / loop;
  sum = 0;
  for (int i = 0; i < loop; i++) sum += (times[i] - mean) * (times[i] - mean);
  double std = sqrt(sum / (loop - 1));
  printf("%.2f %.2f\n", mean, std);
  MPIV_Stop_worker();
}
