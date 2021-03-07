#include "lci.h"
#include "comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#undef MAX_MSG
#define MAX_MSG 8

int main(int argc, char** args) {
  LCI_open();
  LCI_endpoint_t ep;
  LCI_plist_t prop;
  LCI_plist_create(&prop);
  LCI_MT_t mt;
  LCI_MT_init(&mt, 0);
  LCI_plist_set_MT(prop,&mt);
  // LCI_plist_set_completion(LCI_PORT_MESSAGE, LCI_COMPLETION_ONE2ONEL, &prop);
  // LCI_plist_set_completion(LCI_PORT_COMMAND, LCI_COMPLETION_ONE2ONEL, &prop);

  LCI_endpoint_create(0, prop, &ep);
  int rank = LCI_RANK;
  LCI_barrier();

  LCI_syncl_t sync;
  LCI_sync_create(&sync);

  uintptr_t addr, raddr;
  int base_offset = 64 * 1024;
  addr = LCI_get_base_addr(0) + base_offset;

  LCI_sendi(addr, 1-rank, 0, ep);
  LCI_one2one_set_empty(&sync);
  LCI_recvi(&raddr, 1-rank, 0, ep, &sync);
  while (LCI_one2one_test_empty(&sync)) {
    LCI_progress(0, 1);
  }

  long* sbuf = (long*) (addr);
  long* rbuf = (long*) (addr + MAX_MSG);
  memset(sbuf, 1, sizeof(char) * MAX_MSG);
  rbuf[0] = -1;
  LCI_barrier();

  for (int size = sizeof(LCI_short_t); size <= sizeof(LCI_short_t); size <<= 1) {
    for (int i = 0; i < TOTAL; i++) {
      if (rank == 0) {
        while (rbuf[0] == -1)
          LCI_progress(0, 1);
        rbuf[0] = -1;
        LCI_puti(*(LCI_short_t*)sbuf, 1-rank, 0, base_offset + MAX_MSG, 0, ep);
      } else {
        LCI_puti(*(LCI_short_t*)sbuf, 1-rank, 0, base_offset + MAX_MSG, 0, ep);
        while (rbuf[0] == -1)
          LCI_progress(0, 1);
        rbuf[0] = -1;
      }
    }
  }

  LCI_close();
}
