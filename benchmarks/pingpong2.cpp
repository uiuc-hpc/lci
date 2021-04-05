#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "lci.h"
#include "comm_exp.h"

/**
 * Ping-pong benchmark with sendbc/recvbc
 * Touch the data
 *
 * write the buffer before send and read the buffer after recv
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
  LCI_endpoint_init(&ep, 0, prop);
  rank = LCI_RANK;
  size = LCI_NUM_PROCESSES;
  yp_init();

  char* buf;
  LCI_tag_t tag = 99;

  size_t msg_size;
  LCI_comp_t sync;

  _memalign((void**) &buf, 8192, max_size);

  if(rank == 0) {
    print_banner();

    RUN_VARY_MSG({min_size, max_size}, 1, [&](int msg_size, int iter) {

      LCI_recvm(ep, buf, 1 - rank, tag, sync, NULL);
      while (LCI_one2one_test_empty(&sync))
        LCI_progress(0, 1);
      check_buffer(buf, size, 's');

      write_buffer(buf, size, 'r');
      while (LCI_sendm(ep, buf, 1 - rank, tag) != LCI_OK)
        LCI_progress(0, 1);
    });
  } else {
    RUN_VARY_MSG({min_size, max_size}, 0, [&](int msg_size, int iter) {
      write_buffer(buf, size, 's');
      while (LCI_sendm(ep, buf, 1 - rank, tag) != LCI_OK)
        LCI_progress(0, 1);


      LCI_recvm(ep, buf, 1 - rank, tag, sync, NULL);
      while (LCI_one2one_test_empty(&sync))
        LCI_progress(0, 1);
      check_buffer(buf, size, 'r');
    });
  }

  _free(buf);

  LCI_close();
  return EXIT_SUCCESS;
}