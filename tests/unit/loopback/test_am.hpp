// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

#include "lci_internal.hpp"

namespace test_comm_am
{
void test_am_worker_fn(int thread_id, int nmsgs, size_t msg_size,
                       bool allow_retry)
{
  int rank = lci::get_rank_me();
  lci::tag_t tag = thread_id;

  lci::comp_t lcq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);

  void* send_buffer = malloc(msg_size);
  util::write_buffer(send_buffer, msg_size, 'a');

  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    KEEP_RETRY(status, lci::post_am_x(rank, send_buffer, msg_size, lcq, rcomp)
                           .tag(tag)
                           .allow_retry(allow_retry)());

    // poll cqs
    bool lcq_done = status.is_done();
    bool rcq_done = false;
    while (!lcq_done || !rcq_done) {
      lci::progress();
      if (!lcq_done) {
        status = lci::cq_pop(lcq);
        if (status.is_done()) {
          ASSERT_EQ(status.buffer, send_buffer);
          ASSERT_EQ(status.size, msg_size);
          lcq_done = true;
        }
      }
      if (!rcq_done) {
        status = lci::cq_pop(rcq);
        if (status.is_done()) {
          ASSERT_EQ(status.size, msg_size);
          util::check_buffer(status.buffer, msg_size, 'a');
          free(status.buffer);
          rcq_done = true;
        }
      }
    }
  }

  free(send_buffer);
  lci::free_comp(&lcq);
  lci::free_comp(&rcq);
}

void test_am_common(int nthreads, int nmsgs, size_t msg_size, bool allow_retry)
{
  fprintf(stderr, "test_am_common: nthreads=%d nmsgs=%d msg_size=%ld\n",
          nthreads, nmsgs, msg_size);
  util::spawn_threads(nthreads, test_am_worker_fn, nmsgs, msg_size,
                      allow_retry);
}

TEST(COMM_AM, am)
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
      test_am_common(nthread, nmsgs, msg_size, false);
    }
  }

  // Test backlog queue
  const int nthread = 4;
  for (auto& msg_size : msg_sizes) {
    int nmsgs = nmsgs_total / nthread;
    test_am_common(nthread, nmsgs, msg_size, true);
  }

  lci::g_runtime_fina();
}

}  // namespace test_comm_am