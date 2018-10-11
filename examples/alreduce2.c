#include "lc.h"
#include "comm_exp.h"
#include <assert.h>
#include <string.h>

void op_sum(void* dst, void* src, int count)
{
  int* dst_int = (int*) dst;
  int* src_int = (int*) src;
  int i = 0;
  for (i = 0; i < count / sizeof(int); i++) {
    dst_int[i] += src_int[i];
  }
}

lch* mv;

static pthread_t progress_thread;
static volatile int lc_thread_stop;

static void* progress(void* arg __UNUSED__)
{
  while (!lc_thread_stop) {
    while (lc_progress(mv))
      ;
  }
  return 0;
}
int main(int argc, char** args) {
  lc_open(&mv);

  lc_thread_stop = 0;
  pthread_create(&progress_thread, 0, progress, 0);

  int* dst = malloc(128*1024);

  double t1;
  int i;
  for (i = 0; i < TOTAL + SKIP; i++) {
    if (i == SKIP) {
      t1 = wtime();
    }

    lc_alreduce(LC_COL_IN_PLACE, dst, 128*1024, op_sum, mv);
  }

  t1 = wtime() - t1;

  if (lc_id(mv) == 0)
    printf("%.5f (usec)\n", 1e6 * t1 / TOTAL);

  lc_close(mv);
}
