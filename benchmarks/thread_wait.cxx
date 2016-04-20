#include <stdio.h>
#include <thread>
#include <string.h>
#include <assert.h>
#include <atomic>

#include "ult.h"
#include "comm_exp.h"

std::atomic<int> total;

void f1(intptr_t i) {
  total += 1;
  bool* b = (bool*) i;
  __fulting->wait(*b);
}

int nworker = DEFAULT_NUM_WORKER;
int num_threads = DEFAULT_NUM_THREAD;
int total_threads;

void main_task(intptr_t);

int main(int argc, char** args) {
#ifdef USE_ABT
  printf("Init\n");
  ABT_init(argc, args);
#endif

  int nworker = 2;
  if (argc > 1) num_threads = atoi(args[1]);
  printf("Num worker: %d, Num threads: %d\n", nworker, num_threads);

  total_threads = num_threads * nworker;
  worker w[2];
  w[1].start();
  w[0].start_main(main_task, (intptr_t)&w);
  w[1].stop();
}

void main_task(intptr_t arg) {
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
      tid[i] = w[1].spawn(f1, (intptr_t) &b[i]);
    }
    while (total != total_threads) ult_yield();

    t -= wtime();
    for (int i = 0; i < total_threads; i++) {
      tid[i]->resume(b[i]);
    }
    for (int i = 0; i < total_threads; i++) {
      tid[i]->join();
    }
    t += wtime();
  }
  printf("%f\n", 1e6 * t / loop / total_threads);
  delete[] tid;
  delete[] b;
  w[0].stop_main();
#endif
}
