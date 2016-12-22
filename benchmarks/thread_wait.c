#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mv/macro.h"

#include "comm_exp.h"

// #define USE_L1_MASK

#ifdef USE_ABT
#include "mv/helper_abt.h"
#elif defined(USE_PTH)
#include "mv/helper_pth.h"
#else
#include "mv/helper.h"
#endif

volatile int total = 0;
// int nworker = DEFAULT_NUM_WORKER;
// int num_threads = DEFAULT_NUM_THREAD;
int total_threads;
int num_threads;
int num_worker;

void f1(intptr_t i)
{
  set_me_to_(i);
  __sync_fetch_and_add(&total, 1);
  mv_sync* sync = mv_get_sync();
  thread_wait(sync);
}

void main_task(intptr_t);

int main(int argc, char** args)
{
#ifdef USE_ABT
  ABT_init(argc, args);
#endif
  if (argc > 1) num_threads = atoi(args[1]);
  if (argc > 1) num_worker = atoi(args[2]);

  printf("Num worker: %d, Num threads: %d\n", num_worker + 1, num_threads);

  total_threads = num_threads * num_worker;
  MPIV_Start_worker(num_worker + 1, 0);
  return 0;
}

void main_task(intptr_t arg)
{
  mv_thread* tid = (mv_thread*) malloc(total_threads * sizeof(mv_thread));
  set_me_to_(0);

  double t = 0;
  int loop = 100;
  double times[loop];

  for (int tt = 0; tt < loop; tt++) {
    total = 0;
    for (int i = 0; i < total_threads; i++) {
      tid[i] = MPIV_spawn(1 + i % num_worker, f1, 1 + i % num_worker);
    }
    while (total != total_threads) {
      thread_yield();
    };
    // usleep(10);
    printf("%d\n", tt);
    times[tt] = wutime();
    for (int i = 0; i < total_threads; i++) {
      mv_sync* sync = (mv_sync*) tid[i];
      thread_signal(sync);
    }
    times[tt] = (wutime() - times[tt]) / total_threads;
    for (int i = 0; i < total_threads; i++) {
      MPIV_join(tid[i]);
    }
  }

  double sum = 0;
  for (int i = 0; i < loop; i++)
    sum += times[i];
  double mean = sum / loop;
  sum = 0;
  for (int i = 0; i < loop; i++)
    sum += (times[i] - mean) * (times[i] - mean);
  double std = sqrt(sum / (loop - 1));
  printf("%.2f %.2f\n", mean, std);
}
