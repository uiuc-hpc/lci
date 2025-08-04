// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace test_comm_put
{
void test_put_worker_fn(int thread_id, int nmsgs, size_t msg_size)
{
  int rank = lci::get_rank_me();
  lci::tag_t tag = thread_id;

  lci::comp_t cq = lci::alloc_cq();
  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  util::write_buffer(send_buffer, msg_size, 'a');

  lci::mr_t mr = lci::register_memory(recv_buffer, msg_size);
  lci::rmr_t rmr = lci::get_rmr(mr);

  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    KEEP_RETRY(status, lci::post_put_x(rank, send_buffer, msg_size, cq, 0, rmr)
                           .tag(tag)
                           .comp_semantic(lci::comp_semantic_t::network)());

    if (status.is_posted()) {
      KEEP_RETRY(status, lci::cq_pop(cq));
    }

    util::check_buffer(recv_buffer, msg_size, 'a');
  }

  lci::deregister_memory(&mr);
  free(send_buffer);
  free(recv_buffer);
  lci::free_comp(&cq);
}

void test_put_common(int nthreads, int nmsgs, size_t msg_size)
{
  fprintf(stderr, "test_put_common: nthreads=%d nmsgs=%d msg_size=%ld\n",
          nthreads, nmsgs, msg_size);
  util::spawn_threads(nthreads, test_put_worker_fn, nmsgs, msg_size);
}

TEST(COMM_PUT, put)
{
  lci::g_runtime_init();

  const int nmsgs_total = util::NITERS_SMALL;
  const size_t max_bcopy_size = lci::get_max_bcopy_size();
  std::vector<size_t> msg_sizes = {0, 8, max_bcopy_size, max_bcopy_size + 1,
                                   65536};
  std::vector<int> nthreads = {1, util::NTHREADS};

  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();

  for (auto& nthread : nthreads) {
    for (auto& msg_size : msg_sizes) {
      int nmsgs = nmsgs_total / nthread;
      test_put_common(nthread, nmsgs, msg_size);
    }
  }

  lci::g_runtime_fina();
}

}  // namespace test_comm_put