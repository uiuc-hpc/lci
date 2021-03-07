#include "lci.h"
#include "comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

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

  uintptr_t addr;
  int base_offset = 64 * 1024;
  addr = LCI_get_base_addr(0) + base_offset;

  long* sbuf = (long*) (addr);
  long* rbuf = (long*) (addr + LCI_SHORT_SIZE);
  memset(sbuf, 1, sizeof(char) * LCI_SHORT_SIZE);
  rbuf[0] = -1;
  LCI_barrier();

  for (int i = 0; i < TOTAL; i++) {
    LCI_puti(*(LCI_short_t*)sbuf, rank, 0, base_offset + LCI_SHORT_SIZE, 123, ep);
    while (rbuf[0] == -1)
      LCI_progress(0, 1);
    rbuf[0] = -1;
  }

  LCI_close();
}
