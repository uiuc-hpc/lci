#include <assert.h>
#include <stdio.h>
#include <string.h>

// #define USE_L1_MASK

#include "comm_exp.h"

#ifdef USE_ABT
#include "mv/helper_abt.h"
#elif defined(USE_PTH)
#include "mv/helper_pth.h"
#else
#include "mv/helper.h"
#endif

volatile int total;
// int nworker = DEFAULT_NUM_WORKER;
// int num_threads = DEFAULT_NUM_THREAD;
int total_threads;
int num_threads;
int num_worker;

void f1(intptr_t i)
{
  set_me_to_(i);
  for (int i = 0; i < TOTAL; i++) {
    thread_yield();
  }
}

void main_task(intptr_t);

int main(int argc, char** args)
{
#ifdef USE_ABT
  ABT_init(argc, args);
#endif
  // MPIV_Init(&argc, &args);
  if (argc > 1) num_threads = atoi(args[1]);
  if (argc > 1) num_worker = atoi(args[2]);

  printf("Num worker: %d, Num threads: %d\n", nworker, num_threads);

  total_threads = num_threads * num_worker;
  MPIV_Start_worker(num_worker, 0);
  return 0;
}

void main_task(intptr_t arg)
{
#if 1
  mv_thread* tid = malloc(total_threads * sizeof(mv_thread));
  set_me_to_(0);

  double t = 0;
  int loop = 100;
  // for (int tt = 0; tt < loop; tt++) {
  total = 0;
  t -= wutime();
  for (int i = 0; i < total_threads; i++) {
    tid[i] = MPIV_spawn(i % num_worker, f1, i % num_worker);
  }
  for (int i = 0; i < total_threads; i++) {
    MPIV_join(tid[i]);
  }
  t += wutime();
  // }
  printf("%f\n", t / TOTAL / total_threads);
#endif
}
