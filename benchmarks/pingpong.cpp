#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "lci.h"
#include "comm_exp.h"

/**
 * Ping-pong benchmark with sendbc/recvbc
 */

LCI_endpoint_t ep;

int main(int argc, char *argv[]) {
  int rank, size;
  int min_size = 8;
  int max_size = 8192;
  if (argc > 1)
    min_size = atoi(argv[1]);
  if (argc > 2)
    max_size = atoi(argv[2]);

  LCI_open();
  LCI_plist_t prop;
  LCI_plist_create(&prop);
  LCI_MT_t mt;
  LCI_MT_init(&mt, 0);
  LCI_plist_set_MT(prop, &mt);
  LCI_endpoint_create(0, prop, &ep);
  rank = LCI_RANK;
  size = LCI_NUM_PROCESSES;
  yp_init();

  char* s_buf, *r_buf;
  int tag = 99;

  size_t msg_size;
  LCI_syncl_t sync;

  _memalign((void**) &s_buf, 8192, max_size);
  _memalign((void**) &r_buf, 8192, max_size);
  memset(s_buf, 'A', max_size);
  memset(r_buf, 'B', max_size);

  if(rank == 0) {
    print_banner();

    RUN_VARY_MSG({min_size, max_size}, 1, [&](int msg_size, int iter) {
      LCI_one2one_set_empty(&sync);
      LCI_recvbc(r_buf, size, 1-rank, tag, ep, &sync);
      while (LCI_one2one_test_empty(&sync))
        LCI_progress(0, 1);

      while (LCI_sendbc(s_buf, size, 1-rank, tag, ep) != LCI_OK)
        LCI_progress(0, 1);
    });
  } else {
    RUN_VARY_MSG({min_size, max_size}, 0, [&](int msg_size, int iter) {
      while (LCI_sendbc(s_buf, size, 1-rank, tag, ep) != LCI_OK)
        LCI_progress(0, 1);

      LCI_one2one_set_empty(&sync);
      LCI_recvbc(r_buf, size, 1-rank, tag, ep, &sync);
      while (LCI_one2one_test_empty(&sync))
        LCI_progress(0, 1);
    });
  }

  _free(s_buf);
  _free(r_buf);

  LCI_close();
  return EXIT_SUCCESS;
}