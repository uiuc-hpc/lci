#include "lci.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "comm_exp.h"

int total = TOTAL;
int skip = SKIP;

int main(int argc, char** args) {
  LCI_Initialize(1);
  LCI_Endpoint ep;
  LCI_Property prop;
  LCI_Property_create(&prop);
  LCI_Endpoint_create(0, prop, &ep);

  int rank = LCI_Rank();
  int tag = 99;

  LCI_Request req;
  LCI_Sync sync;
  LCI_Sync_create(&sync);

  LCI_Sync lsync;
  LCI_Sync_create(&lsync);

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
        LCI_Sync_reset(&lsync);
        LCI_Sendl(src_buf, size, 1-rank, tag, ep, lsync);
        while (!LCI_Sync_test(lsync))
          LCI_Progress(0, 1);
        LCI_Sync_reset(&sync);
        while (LCI_Recvl(dst_buf, size, 1-rank, tag, ep, sync, &req) != LCI_OK)
          LCI_Progress(0, 1);
        while (!LCI_Sync_test(sync))
          LCI_Progress(0, 1);
        if (i == 0) {
          for (int j = 0; j < size; j++)
            assert(((char*) src_buf)[j] == 'a' && ((char*)dst_buf)[j] == 'a');
        }
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
        LCI_Sync_reset(&sync);
        while (LCI_Recvl(dst_buf, size, 1-rank, tag, ep, sync, &req) != LCI_OK)
          LCI_Progress(0, 1);
        while (!LCI_Sync_test(sync))
          LCI_Progress(0, 1);
        LCI_Sync_reset(&lsync);
        LCI_Sendl(src_buf, size, 1-rank, tag, ep, lsync);
        while (!LCI_Sync_test(lsync))
          LCI_Progress(0, 1);
      }
    }
  }
  lc_pm_barrier();
  LCI_Finalize();
}
