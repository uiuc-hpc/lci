#include <assert.h>
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <thread>

#include "comm_exp.h"
#include "ult.h"

std::atomic<int> total;
int nworker = DEFAULT_NUM_WORKER;
int num_threads = DEFAULT_NUM_THREAD;
int total_threads;

void f1(intptr_t i)
{
  total += 1;
  bool* b = (bool*)i;
  __fulting->wait(*b);
}

void main_task(intptr_t);

int main(int argc, char** args)
{
#ifdef USE_ABT
  ABT_init(argc, args);
#endif

  if (argc > 1) num_threads = atoi(args[1]);
  if (argc > 2) nworker = atoi(args[2]);
  printf("Num worker: %d, Num threads: %d\n", nworker, num_threads);

  total_threads = num_threads * nworker;
  worker w[nworker + 1];
  for (int i = 1; i < nworker + 1; i++) w[i].start();
  w[0].start_main(main_task, (intptr_t)&w);
  for (int i = 1; i < nworker + 1; i++) w[i].stop();
}

void main_task(intptr_t arg)
{
#if 1
  worker* w = (worker*)arg;
  thread* tid = new thread[total_threads];
  double t = 0;
  bool* b = new bool[total_threads];
  int loop = 100;
  for (int tt = 0; tt < loop; tt++) {
    memset(b, 0, total_threads * sizeof(bool));
    total = 0;
    for (int i = 0; i < total_threads; i++) {
      tid[i] = w[1].spawn(f1, (intptr_t)&b[i]);
    }
    while (total != total_threads) {
    };
    t -= wtime();
    for (int i = 0; i < total_threads; i++) {
      tid[i]->resume(b[i]);
    }
    t += wtime();
    for (int i = 0; i < total_threads; i++) {
      tid[i]->join();
    }
  }
  printf("%f\n", 1e6 * t / loop / total_threads);
  delete[] tid;
  delete[] b;
  w[0].stop_main();
#endif
}
