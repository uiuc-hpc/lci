#include "lci.h"
#include "comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#undef MAX_MSG
#define MAX_MSG (8 * 1024)

int main(int argc, char** args) {
  LCI_open();
  LCI_endpoint_t ep;
  LCI_plist_t prop;
  LCI_plist_create(&prop);
  LCI_MT_t mt;
  LCI_MT_init(&mt, 0);
  LCI_plist_set_MT(prop,&mt);
  // LCI_plist_set_completion(LCI_PORT_MESSAGE, LCI_COMPLETION_SYNC, &prop);
  // LCI_plist_set_completion(LCI_PORT_COMMAND, LCI_COMPLETION_SYNC, &prop);

  LCI_endpoint_init(&ep, 0, prop);
  int rank = LCI_RANK;
  LCI_barrier();

  LCI_comp_t sync;
  LCI_sync_create(&sync);

  uintptr_t addr, raddr;
  int base_offset = 64 * 1024;
  addr = LCI_get_base_addr(0) + base_offset;

  LCI_sends(ep, addr, rank, 0);
  LCI_one2one_set_empty(&sync);
  LCI_recvs(ep, &raddr, 0, &sync);
  while (LCI_one2one_test_empty(&sync)) {
    LCI_progress(0, 1);
  }

  long* sbuf = (long*) (addr);
  long* rbuf = (long*) (addr + MAX_MSG);
  memset(sbuf, 1, sizeof(char) * MAX_MSG);
  rbuf[0] = -1;
  LCI_barrier();

  for (int size = 8; size <= MAX_MSG; size <<= 1) {
    for (int i = 0; i < TOTAL; i++) {

      LCI_putbc(sbuf, size, rank, 0, base_offset + MAX_MSG, 99, ep);
      while (rbuf[0] == -1)
        LCI_progress(0, 1);
      rbuf[0] = -1;
    }
  }

  LCI_close();
  return 0;
}
