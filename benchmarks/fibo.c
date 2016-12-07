#include <assert.h>
#include <atomic>
#include <stdio.h>
#include <string.h>

#include "comm_exp.h"
#include "ult.h"
#include "ult/fult/fult.h"

long* times;
long* fibo;
fworker** w;

__thread struct tls_t tlself;

typedef struct thread_data_t {
  long val;
  long ret;
} thread_data_t ;

int number;
int nfworker;

void ffibo(intptr_t arg)
{
  thread_data_t* td = (thread_data_t*)arg;
  if (td->val <= 1) {
    td->ret = td->val;
  } else {
    thread_data_t data[2];
    data[0].val = td->val - 1;
    data[1].val = td->val - 2;
    fthread* s1 =
        fworker_spawn(w[(tlself.worker->id + 1) % nfworker], ffibo, (intptr_t)&data[0], F_STACK_SIZE);
    fthread* s2 =
        fworker_spawn(w[(tlself.worker->id + 2) % nfworker], ffibo, (intptr_t)&data[1], F_STACK_SIZE);
    fthread_join(s1);
    fthread_join(s2);
    td->ret = data[0].ret + data[1].ret;
  }
}

fworker* random_worker()
{
  int p = rand() % nfworker;
  // printf("pick %d\n", p);
  return w[p];
}

void main_task(intptr_t args)
{
  fworker** w = (fworker**)args;
  double t = wtime();
  thread_data_t data = {number, 0};
  for (int tt = 0; tt < TOTAL_LARGE; tt++) {
    ffibo((intptr_t)&data);
  }
  printf("RESULT: %lu %f\n", data.ret,
         (double)1e6 * (wtime() - t) / TOTAL_LARGE);
  fworker_stop_main(w[0]);
}

int main(int argc, char** args)
{
#ifdef USE_ABT
  ABT_init(argc, args);
#endif
  if (argc < 3) {
    printf("Usage: %s <nfworker> <number>\n", args[0]);
    return 1;
  }
  number = atoi(args[1]);
  nfworker = atoi(args[2]);
  w = malloc(sizeof(fworker*) * nfworker);
  fworker_init(&w[0]);
  w[0]->id = 0;
  for (int i = 1; i < nfworker; i++) {
    fworker_init(&w[i]);
    w[i]->id = i;
    fworker_start(w[i]);
  }
  fworker_start_main(w[0], main_task, (intptr_t)w);
  for (int i = 1; i < nfworker; i++) {
    fworker_stop(w[i]);
  }
  return 0;
}
