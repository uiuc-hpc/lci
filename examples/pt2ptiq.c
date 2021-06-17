#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

#undef MAX_MSG
#define MAX_MSG 8

int total = TOTAL;
int skip = SKIP;

int main(int argc, char** args) {
  LCI_open();
  LCI_endpoint_t ep;
  LCI_plist_t plist;
  LCI_comp_t cq;
  LCI_plist_create(&plist);
  LCI_queue_create(0, &cq);
  LCI_plist_set_CQ(plist,&cq);
  LCI_plist_set_completion(plist,LCI_PORT_COMMAND, LCI_COMPLETION_SYNC);
  LCI_plist_set_completion(plist,LCI_PORT_MESSAGE, LCI_COMPLETION_QUEUE);
  LCI_MT_t mt;
  LCI_MT_init(&mt, 0);
  LCI_plist_set_MT(plist,&mt);

  LCI_endpoint_init(&ep, LCI_UR_DEVICE, plist);
  LCI_barrier();

  int rank = LCI_RANK;
  LCI_tag_t tag = 99;

  LCI_request_t* req_ptr;
  LCI_comp_t sync;

  double t1 = 0;
  size_t alignment = sysconf(_SC_PAGESIZE);
  void* src_buf = 0;
  void* dst_buf = 0;
  posix_memalign(&src_buf, alignment, MAX_MSG);
  posix_memalign(&dst_buf, alignment, MAX_MSG);

  if (rank == 0) {
    for (int size = sizeof(LCI_short_t); size <= sizeof(LCI_short_t); size <<= 1) {
      memset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);

      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        if (i == skip) t1 = wtime();
        LCI_sends(ep, *(LCI_short_t*)src_buf, 1 - rank, tag);
        LCI_recvs(ep, dst_buf, tag, sync);
        while (LCI_queue_pop(cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        if (i == 0) {
          for (int j = 0; j < size; j++)
            assert(((char*) src_buf)[j] == 'a' && ((char*)dst_buf)[j] == 'a');
        }
        LCI_mbuffer_free(0, req_ptr->data.buffer.start);
      }

      t1 = 1e6 * (wtime() - t1) / total / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  } else {
    for (int size = sizeof(LCI_short_t); size <= sizeof(LCI_short_t); size <<= 1) {
      memset(src_buf, 'a', size);
      memset(dst_buf, 'b', size);
      if (size > LARGE) { total = TOTAL_LARGE; skip = SKIP_LARGE; }

      for (int i = 0; i < total + skip; i++) {
        LCI_recvs(ep, dst_buf, tag, sync);
        while (LCI_queue_pop(cq, &req_ptr) == LCI_ERR_RETRY)
          LCI_progress(LCI_UR_DEVICE);
        LCI_mbuffer_free(0, req_ptr->data.buffer.start);
        LCI_sends(ep, *(LCI_short_t*)src_buf, 1 - rank, tag);
      }
    }
  }
  LCI_close();
}
