#include "lc.h"
#include "comm_exp.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>

void op_sum(void* dst, void* src, size_t count)
{
  int* dst_int = (int*) dst;
  int* src_int = (int*) src;
  int i = 0;
  for (i = 0; i < count / sizeof(int); i++) {
    dst_int[i] += src_int[i];
  }
}
volatile int cont = 1;

void* poll_thread(void* arg)
{
    while (cont) {
        lc_progress(0);
        sched_yield();
    }
    return 0;
}

size_t total = TOTAL;
size_t skip = SKIP;

int main(int argc, char** args) {
  lc_ep ep;
  lc_init(1, LC_EXPL_SYNC, &ep);
  // pthread_t thread;
  // pthread_create(&thread, NULL, poll_thread, (void*) ep);

  int t;
  lc_get_proc_num(&t);

  int* dst;
  posix_memalign((void**) &dst, 4096, 1 << 22);
  lc_colreq colreq;

  double t1;
  unsigned i;
  for (size_t size = 1; size < (1<<22) / 4; size <<= 1) {
    if (size >= LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    for (i = 0; i < total + skip; i++) {
      if (i == skip) {
        t1 = wtime();
        for (int j = 0; j < size; j++)
          dst[j] = t;
      }

      lc_ialreduce(LC_COL_IN_PLACE, dst, size * sizeof(int), op_sum, ep, &colreq);
      while (colreq.flag == 0) {
        lc_col_progress(&colreq);
        lc_progress(0);
      }

      if (i == skip && size == 1) {
        int sum;
        lc_get_num_proc(&sum);
        sum = (sum * (sum-1) / 2);
        for (int j = 0; j < size; j++)
          assert(dst[j] == sum);
      }
    }

    lc_ibarrier(ep, &colreq);
    while (colreq.flag == 0) {
      lc_col_progress(&colreq);
      lc_progress(0);
    }

    t1 = wtime() - t1;
    if (t == 0)
      printf("%10d %10.2f \n", size * sizeof(int), 1e6 * t1 / total);
  }

  // cont = 0;
  // pthread_join(thread, 0);

  lc_finalize();
}
