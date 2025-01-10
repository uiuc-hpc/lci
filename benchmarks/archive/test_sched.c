#include <sched.h>
#include <stdio.h>
#include <pthread.h>
#include "comm_exp.h"

__thread double y;

double get_value() __attribute__((noinline));

double get_value() { return y; }

void* f(void* x)
{
  double t1 = wtime();
  printf("%d\n", sched_getcpu());
  double f;
  for (int i = 0; i < 1000000; i++) f += sqrt(get_value());
  printf("%.8f\n", (wtime() - t1) / 1000000);
  return 0;
}

typedef struct {
  char __padding__[0];
} x;

int main()
{
  printf("%d\n", sizeof(x));
  pthread_t t;
  pthread_create(&t, 0, f, 0);
  pthread_join(t, 0);
  return 0;
}
