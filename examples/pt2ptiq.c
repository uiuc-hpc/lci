#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG 128

int total = TOTAL;
int skip = SKIP;

int main(int argc, char** args) {
  LCI_initialize(&argc, &args);
  LCI_endpoint_t ep;
  LCI_PL_t prop;
  LCI_CQ_t cq;
  LCI_PL_create(&prop);
  LCI_CQ_create(0, &cq);
  LCI_PL_set_cq(&cq, &prop);
  LCI_PL_set_completion(LCI_PORT_COMMAND, LCI_COMPLETION_ONE2ONEL, &prop);
  LCI_PL_set_completion(LCI_PORT_MESSAGE, LCI_COMPLETION_QUEUE, &prop);
  LCI_MT_t mt;
  LCI_MT_create(0, &mt);
  LCI_PL_set_mt(&mt, &prop);

  LCI_endpoint_create(0, prop, &ep);
  LCI_PM_barrier();

  int rank = LCI_RANK;
  int tag = 99;

  LCI_request_t* req_ptr;
  LCI_syncl_t sync;

  double t1 = 0;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* src_buf = 0;
  void* dst_buf = 0;
  posix_memalign(&src_buf, alignment, MAX_MSG);
  posix_memalign(&dst_buf, alignment, MAX_MSG);

  if (rank == 0) {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        LCI_sendi(src_buf, size, 1-rank, tag, ep);
        LCI_recvi(dst_buf, size, 1-rank, tag, ep, &sync);
        while (LCI_CQ_dequeue(&cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(0, 1);
        if (i == 0) {
          for (int j = 0; j < size; j++)
            assert(((char*) src_buf)[j] == 'a' && ((char*)dst_buf)[j] == 'a');
        }
        LCI_request_free(ep, 1, &req_ptr);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = MIN_MSG; size <= MAX_MSG; size <<= 1) {
      memset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        LCI_recvi(dst_buf, size, 1-rank, tag, ep, &sync);
        while (LCI_CQ_dequeue(&cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(0, 1);
        LCI_request_free(ep, 1, &req_ptr);
        LCI_sendi(src_buf, size, 1-rank, tag, ep);
      }
    }
  }
  LCI_finalize();
}
