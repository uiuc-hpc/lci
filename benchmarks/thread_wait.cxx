#include <stdio.h>
#include <thread>
#include <string.h>
#include <assert.h>
#include <atomic>

#include "fult.h"
#include "comm_exp.h"

std::atomic<int> total;

void f1(intptr_t i) {
  total += 1;
  fult_wait();
}

int nworker = DEFAULT_NUM_WORKER;
int num_threads = DEFAULT_NUM_THREAD;
int total_threads;

void main_task(intptr_t);

int main(int argc, char** args) {
  if (argc > 1) nworker = atoi(args[1]);
  if (argc > 2) num_threads = atoi(args[2]);

  total_threads = num_threads * nworker;

  worker w[nworker];
  for (int i = 1; i < nworker; i++) {
    w[i].start();
  }

  w[0].start_main(main_task, (intptr_t) w);
}

void main_task(intptr_t arg) {
  worker* w = (worker*) arg;
  fult_t tid[total_threads];
  double  t = 0;
  for (int tt = 0; tt < TOTAL; tt++) {
    total = 0;
    for (int i = 0; i < total_threads; i++) {
      tid[i] = w[i % nworker].spawn(f1, i);
    }
    while (total != num_threads * nworker) fult_yield();
    t -= wtime();
    for (int i = 0; i < total_threads; i++) {
      w[i % nworker].schedule(tid[i]->id());
    }
    for (int i = 0; i < total_threads; i++) {
      w[i % nworker].join(tid[i]);
    }
    t += wtime();
  }
  printf("%f\n", 1e6 * t / TOTAL / total_threads);

  for (int i = 1; i < nworker; i++) {
    w[i].stop();
  }
  w[0].stop_main();
}
