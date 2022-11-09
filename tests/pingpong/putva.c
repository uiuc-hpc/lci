#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "lci.h"
#include "comm_exp.h"

int main(int argc, char** args)
{
  int lbuffers_num = 8;
  int piggy_back_size = (int)LCI_get_iovec_piggy_back_size(lbuffers_num);
  int lbuffer_length = 64 * 1024;
  int loop = 1000;

  LCI_initialize();
  LCI_endpoint_t ep = LCI_UR_ENDPOINT;  // we can directly use the default ep
  LCI_comp_t send_cq;
  LCI_queue_create(LCI_UR_DEVICE, &send_cq);

  int rank = LCI_RANK;
  int peer_rank = ((1 - LCI_RANK % 2) + LCI_RANK / 2 * 2) % LCI_NUM_PROCESSES;
  LCI_tag_t tag = 99;

  LCI_iovec_t iovec;
  iovec.piggy_back.address = malloc(piggy_back_size);
  write_buffer(iovec.piggy_back.address, piggy_back_size, 'p');
  iovec.piggy_back.length = piggy_back_size;
  iovec.count = lbuffers_num;
  iovec.lbuffers = malloc(iovec.count * sizeof(LCI_lbuffer_t));
  for (int i = 0; i < iovec.count; ++i) {
    iovec.lbuffers[i].address = malloc(lbuffer_length);
    iovec.lbuffers[i].length = lbuffer_length;
    iovec.lbuffers[i].segment = LCI_SEGMENT_ALL;
    write_buffer(iovec.lbuffers[i].address, lbuffer_length, 'l' + i);
  }
  LCI_request_t request;
  LCI_barrier();

  if (rank % 2 == 0) {
    for (int i = 0; i < loop; i++) {
      while (LCI_putva(ep, iovec, send_cq, peer_rank, tag,
                       LCI_DEFAULT_COMP_REMOTE, NULL) == LCI_ERR_RETRY)
        LCI_progress(LCI_UR_DEVICE);
      while (LCI_queue_pop(send_cq, &request) == LCI_ERR_RETRY)
        LCI_progress(LCI_UR_DEVICE);
      while (LCI_queue_pop(LCI_UR_CQ, &request) == LCI_ERR_RETRY)
        LCI_progress(LCI_UR_DEVICE);
      for (int j = 0; j < request.data.iovec.count; ++j) {
        check_buffer(request.data.iovec.lbuffers[j].address,
                     request.data.iovec.lbuffers[j].length, 'l' + j);
        LCI_lbuffer_free(request.data.iovec.lbuffers[j]);
      }
      free(request.data.iovec.lbuffers);
      check_buffer(request.data.iovec.piggy_back.address,
                   request.data.iovec.piggy_back.length, 'p');
      free(request.data.iovec.piggy_back.address);
    }
  } else {
    for (int i = 0; i < loop; i++) {
      while (LCI_queue_pop(LCI_UR_CQ, &request) == LCI_ERR_RETRY)
        LCI_progress(LCI_UR_DEVICE);
      for (int j = 0; j < request.data.iovec.count; ++j) {
        check_buffer(request.data.iovec.lbuffers[j].address,
                     request.data.iovec.lbuffers[j].length, 'l' + j);
        LCI_lbuffer_free(request.data.iovec.lbuffers[j]);
      }
      free(request.data.iovec.lbuffers);
      check_buffer(request.data.iovec.piggy_back.address,
                   request.data.iovec.piggy_back.length, 'p');
      free(request.data.iovec.piggy_back.address);
      while (LCI_putva(ep, iovec, send_cq, peer_rank, tag,
                       LCI_DEFAULT_COMP_REMOTE, NULL) == LCI_ERR_RETRY)
        LCI_progress(LCI_UR_DEVICE);
      while (LCI_queue_pop(send_cq, &request) == LCI_ERR_RETRY)
        LCI_progress(LCI_UR_DEVICE);
    }
  }
  for (int j = 0; j < iovec.count; ++j) {
    free(iovec.lbuffers[j].address);
  }
  free(iovec.lbuffers);
  free(iovec.piggy_back.address);
  LCI_finalize();
  return 0;
}
