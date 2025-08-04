// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace test_comm_sendrecv
{
void test_sendrecv_worker_fn(int thread_id, int nmsgs, size_t msg_size,
                             bool register_memory = false,
                             bool expected_msg = true)
{
  int rank = lci::get_rank_me();
  lci::tag_t tag = thread_id;

  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();

  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  lci::mr_t send_mr, recv_mr;
  if (register_memory) {
    send_mr = lci::register_memory(send_buffer, msg_size);
    recv_mr = lci::register_memory(recv_buffer, msg_size);
  }

  for (int i = 0; i < nmsgs; i++) {
    util::write_buffer(send_buffer, msg_size, 'a');
    util::write_buffer(recv_buffer, msg_size, 'b');
    bool poll_send = false, poll_recv = false;
    lci::status_t status;
    if (expected_msg) {
      KEEP_RETRY(status, lci::post_recv_x(rank, recv_buffer, msg_size, tag, rcq)
                             .mr(recv_mr)());
      if (status.is_posted()) {
        poll_recv = true;
      }
      KEEP_RETRY(status, lci::post_send_x(rank, send_buffer, msg_size, tag, scq)
                             .mr(send_mr)());
      if (status.is_posted()) {
        poll_send = true;
      }
    } else {
      KEEP_RETRY(status, lci::post_send_x(rank, send_buffer, msg_size, tag, scq)
                             .mr(send_mr)());
      if (status.is_posted()) {
        poll_send = true;
      }
      KEEP_RETRY(status, lci::post_recv_x(rank, recv_buffer, msg_size, tag, rcq)
                             .mr(recv_mr)());
      if (status.is_posted()) {
        poll_recv = true;
      }
    }

    if (poll_send) {
      // poll send cq
      do {
        lci::progress();
        status = lci::cq_pop(scq);
      } while (status.is_retry());
    }
    // poll recv cq
    if (poll_recv) {
      do {
        status = lci::cq_pop(rcq);
        lci::progress();
      } while (status.is_retry());
    }
    util::check_buffer(recv_buffer, msg_size, 'a');
  }
  if (register_memory) {
    lci::deregister_memory(&send_mr);
    lci::deregister_memory(&recv_mr);
  }
  free(send_buffer);
  free(recv_buffer);

  lci::free_comp(&rcq);
  lci::free_comp(&scq);
}

void test_sendrecv_common(int nthreads, int nmsgs, size_t msg_size,
                          bool register_memory, bool expected_msg)
{
  fprintf(stderr,
          "test_sendrecv_common: nthreads=%d nmsgs=%d msg_size=%ld "
          "register_memory=%d expected_msg=%d\n",
          nthreads, nmsgs, msg_size, register_memory, expected_msg);
  util::spawn_threads(nthreads, test_sendrecv_worker_fn, nmsgs, msg_size,
                      register_memory, expected_msg);
}

TEST(COMM_SENDRECV, sendrecv)
{
  lci::g_runtime_init();

  const int nmsgs_total = util::NITERS_SMALL;
  const size_t max_bcopy_size = lci::get_max_bcopy_size();
  std::vector<size_t> msg_sizes = {0, 8, max_bcopy_size, max_bcopy_size + 1,
                                   65536};
  std::vector<int> nthreads = {1, util::NTHREADS};

  // basic setting
  for (auto& nthread : nthreads) {
    for (auto& msg_size : msg_sizes) {
      int nmsgs = nmsgs_total / nthread;
      test_sendrecv_common(nthread, nmsgs, msg_size, false, true);
    }
  }

  // register memory
  for (auto& nthread : nthreads) {
    for (auto& msg_size : msg_sizes) {
      int nmsgs = nmsgs_total / nthread;
      test_sendrecv_common(nthread, nmsgs, msg_size, true, true);
    }
  }

  // unexpected recv
  for (auto& nthread : nthreads) {
    for (auto& msg_size : msg_sizes) {
      int nmsgs = nmsgs_total / nthread;
      test_sendrecv_common(nthread, nmsgs, msg_size, false, false);
    }
  }

  lci::g_runtime_fina();
}

}  // namespace test_comm_sendrecv