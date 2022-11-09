#include "lci.h"
#include "comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#undef MAX_MSG
#define MAX_MSG 8

int main(int argc, char** args)
{
  LCI_open();
  LCI_endpoint_t ep;
  LCI_plist_t plist;
  LCI_plist_create(&plist);
  LCI_MT_t mt;
  LCI_MT_init(&mt, 0);
  LCI_plist_set_MT(plist, &mt);
  // LCI_plist_set_completion(LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC, &plist);
  // LCI_plist_set_completion(LCI_PORT_COMMAND, LCI_COMPLETION_SYNC, &plist);

  LCI_endpoint_init(&ep, LCI_UR_DEVICE, plist);
  int rank = LCI_RANK;
  LCI_barrier();

  LCI_comp_t sync;
  LCI_sync_create(&sync);

  uintptr_t addr, raddr;
  int base_offset = 64 * 1024;
  addr = LCI_get_base_addr(0) + base_offset;

  LCI_sends(ep, addr, 1 - rank, 0);
  LCI_one2one_set_empty(&sync);
  LCI_recvs(ep, &raddr, 0, &sync);
  while (LCI_one2one_test_empty(&sync)) {
    LCI_progress(LCI_UR_DEVICE);
  }

  long* sbuf = (long*)(addr);
  long* rbuf = (long*)(addr + MAX_MSG);
  memset(sbuf, 1, sizeof(char) * MAX_MSG);
  rbuf[0] = -1;
  LCI_barrier();

  double t1;

  for (int size = sizeof(LCI_short_t); size <= sizeof(LCI_short_t);
       size <<= 1) {
    for (int i = 0; i < TOTAL + SKIP; i++) {
      if (i == SKIP) t1 = wtime();
      if (rank == 0) {
        while (rbuf[0] == -1) LCI_progress(LCI_UR_DEVICE);
        rbuf[0] = -1;
        LCI_puts(ep, *(LCI_short_t*)sbuf, 1 - rank, 0, 0);
      } else {
        LCI_puts(ep, *(LCI_short_t*)sbuf, 1 - rank, 0, 0);
        while (rbuf[0] == -1) LCI_progress(LCI_UR_DEVICE);
        rbuf[0] = -1;
      }
    }

    if (rank == 0) {
      t1 = 1e6 * (wtime() - t1) / TOTAL / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  }

  LCI_close();
}
