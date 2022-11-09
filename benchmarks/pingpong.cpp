#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "lci.h"
#include "comm_exp.h"

/**
 * Ping-pong benchmark with sendbc/recvbc
 */

LCI_endpoint_t ep;

int main(int argc, char* argv[])
{
  int min_size = 8;
  int max_size = 8192;
  bool touch_data = false;
  if (argc > 1) min_size = atoi(argv[1]);
  if (argc > 2) max_size = atoi(argv[2]);
  if (argc > 3) touch_data = atoi(argv[3]);

  LCI_initialize();
  ep = LCI_UR_ENDPOINT;

  int rank = LCI_RANK;
  int nranks = LCI_NUM_PROCESSES;
  int peer_rank = (rank + nranks / 2) % nranks;
  LCI_tag_t tag = 99;

  LCI_comp_t cq;
  LCI_queue_create(0, &cq);

  LCI_mbuffer_t mbuffer;
  LCI_request_t request;

  yp_init();
  LCI_barrier();

  if (rank < nranks / 2) {
    print_banner();

    RUN_VARY_MSG(
        {min_size, max_size}, 1,
        [&](int msg_size, int iter) {
          while (LCI_mbuffer_alloc(LCI_UR_DEVICE, &mbuffer) == LCI_ERR_RETRY)
            LCI_progress(LCI_UR_DEVICE);
          if (touch_data) write_buffer((char*)mbuffer.address, msg_size, 's');
          mbuffer.length = msg_size;
          while (LCI_sendmn(ep, mbuffer, peer_rank, tag) == LCI_ERR_RETRY)
            LCI_progress(LCI_UR_DEVICE);

          LCI_recvmn(ep, peer_rank, tag, cq, NULL);
          while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY)
            LCI_progress(LCI_UR_DEVICE);
          assert(request.data.mbuffer.length == msg_size);
          if (touch_data)
            check_buffer((char*)request.data.mbuffer.address, msg_size, 's');
          LCI_mbuffer_free(request.data.mbuffer);
        },
        {rank % (nranks / 2), nranks / 2});
  } else {
    RUN_VARY_MSG(
        {min_size, max_size}, 0,
        [&](int msg_size, int iter) {
          LCI_recvmn(ep, peer_rank, tag, cq, NULL);
          while (LCI_queue_pop(cq, &request) == LCI_ERR_RETRY)
            LCI_progress(LCI_UR_DEVICE);
          assert(request.data.mbuffer.length == msg_size);
          if (touch_data)
            check_buffer((char*)request.data.mbuffer.address, msg_size, 's');
          LCI_mbuffer_free(request.data.mbuffer);

          while (LCI_mbuffer_alloc(LCI_UR_DEVICE, &mbuffer) == LCI_ERR_RETRY)
            LCI_progress(LCI_UR_DEVICE);
          if (touch_data) write_buffer((char*)mbuffer.address, msg_size, 's');
          mbuffer.length = msg_size;
          while (LCI_sendmn(ep, mbuffer, peer_rank, tag) == LCI_ERR_RETRY)
            LCI_progress(LCI_UR_DEVICE);
        },
        {rank % (nranks / 2), nranks / 2});
  }

  LCI_queue_free(&cq);
  LCI_finalize();
  return EXIT_SUCCESS;
}