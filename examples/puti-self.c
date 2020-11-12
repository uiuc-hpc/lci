#include "lci.h"
#include "comm_exp.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#undef MAX_MSG
#define MAX_MSG 8

int main(int argc, char** args) {
  LCI_Init(&argc, &args);
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
  LCI_PM_barrier();

  LCI_syncl_t sync;
  LCI_sync_create(&sync);

  uintptr_t addr;
  int base_offset = 64 * 1024;
  addr = LCI_get_base_addr(0) + base_offset;

  long* sbuf = (long*) (addr);
  long* rbuf = (long*) (addr + MAX_MSG);
  memset(sbuf, 1, sizeof(char) * MAX_MSG);
  rbuf[0] = -1;
  LCI_PM_barrier();

  double t1;

  for (int size = sizeof(LCI_ivalue_t); size <= sizeof(LCI_ivalue_t); size <<= 1) {
    for (int i = 0; i < TOTAL+SKIP; i++) {
      if (i == SKIP)
        t1 = wtime();
      LCI_puti(*(LCI_ivalue_t*)sbuf, rank, 0, base_offset + MAX_MSG, 123, ep);
      while (rbuf[0] == -1)
        LCI_progress(0, 1);
      rbuf[0] = -1;
    }

    if (rank == 0) {
      t1 = 1e6 * (wtime() - t1) / TOTAL / 2;
      printf("%10.d %10.3f\n", size, t1);
    }
  }

  LCI_Free();
}
