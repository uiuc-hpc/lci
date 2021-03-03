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
  LCI_PL_t prop;
  LCI_PL_create(&prop);
  LCI_MT_t mt;
  LCI_MT_init(&mt, 0);
  LCI_PL_set_MT(prop,&mt);
  // LCI_PL_set_completion(LCI_PORT_MESSAGE, LCI_COMPLETION_ONE2ONEL, &prop);
  // LCI_PL_set_completion(LCI_PORT_COMMAND, LCI_COMPLETION_ONE2ONEL, &prop);

  LCI_endpoint_create(0, prop, &ep);
  int rank = LCI_RANK;
  LCI_barrier();

  LCI_syncl_t sync;
  LCI_sync_create(&sync);

  uintptr_t addr, raddr;
  int base_offset = 64 * 1024;
  addr = LCI_get_base_addr(0) + base_offset;

  LCI_sendi(addr, rank, 0, ep);
  LCI_one2one_set_empty(&sync);
  LCI_recvi(&raddr, rank, 0, ep, &sync);
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
}
