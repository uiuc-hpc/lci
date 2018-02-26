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

void* poll_thread(void* mv)
{
    while (cont)
        lc_progress((lch*)mv);
    return 0;
}

size_t total = TOTAL;
size_t skip = SKIP;

int main(int argc, char** args) {
  lch* mv;
  lc_open(&mv, 0);
  pthread_t thread;
  pthread_create(&thread, NULL, poll_thread, (void*) mv);

  int t = lc_id(mv);
  int* dst;
  posix_memalign((void**) &dst, 4096, 1 << 22);

  double t1;
  int i;
  for (size_t size = 1; size < (1<<22) / 4; size <<= 1) {
    if (size >= LARGE) {
      total = TOTAL_LARGE;
      skip = SKIP_LARGE;
    }
    for (i = 0; i < total + skip; i++) {
      if (i == skip) {
        t1 = wtime();
        dst[0] = t;
      }

      lc_alreduce(LC_COL_IN_PLACE, dst, size * sizeof(int), op_sum, mv);

      if (i == skip && size == 1) {
        int j;
        int sum = lc_size(mv);
        sum = (sum * (sum-1) / 2);
        assert(dst[0] == sum);
      }
    }
    lc_barrier(mv);
    t1 = wtime() - t1;
    if (t == 0)
      printf("%10d %10.5f \n", size * sizeof(int), 1e6 * t1 / total);
  }

  cont = 0;
  pthread_join(thread, 0);

  lc_close(mv);
}
