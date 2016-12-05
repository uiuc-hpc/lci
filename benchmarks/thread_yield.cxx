#include <assert.h>
#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

#include "comm_exp.h"
#include "ult.h"

void f1(intptr_t i)
{
  for (int i = 0; i < TOTAL; i++) ult_yield();
}

int compare(const void* a, const void* b) { return (*(long*)a - *(long*)b); }
int num_threads, num_worker, total_threads;
void main_task(intptr_t);

int main(int argc, char** args)
{
#if USE_ABT
  ABT_init(argc, args);
#endif

  if (argc < 2) {
    printf("Usage %s <num_workers> <num_threads>\n", args[0]);
  }
  num_worker = DEFAULT_NUM_WORKER;
  num_threads = DEFAULT_NUM_THREAD;
  // if (argc > 1) num_worker = atoi(args[1]);
  // if (argc > 2) num_threads = atoi(args[2]);
  num_worker = 1;
  num_threads = atoi(args[1]);
  printf("Num worker: %d, Num threads: %d\n", num_worker, num_threads);

  total_threads = num_threads * num_worker;

  worker w[num_worker];

  for (int i = 1; i < num_worker; i++) {
    w[i].start();
  }

  srand(0);
  w[0].start_main(main_task, (intptr_t)w);
  return 0;
}

void main_task(intptr_t arg)
{
  worker* w = (worker*)arg;
  double t = wtime();
  thread tid[num_threads * num_worker];
  for (int i = 0; i < num_threads * num_worker; i++) {
    tid[i] = w[i % num_worker].spawn(f1, i);
  }
  for (int i = 0; i < num_threads * num_worker; i++) {
    tid[i]->join();
  }
  t = wtime() - t;
  printf("%.5f\n", 1e6 * t / TOTAL / total_threads);

  for (int i = 1; i < num_worker; i++) {
    w[i].stop();
  }
  w[0].stop_main();
}
