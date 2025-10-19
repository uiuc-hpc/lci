// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: NCSA

namespace test_comm_putImm
{
void test_putImm_worker_fn(int thread_id, int nmsgs, size_t msg_size)
{
  int rank = lci::get_rank_me();
  lci::tag_t tag = thread_id;

  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);

  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  util::write_buffer(send_buffer, msg_size, 'a');

  lci::mr_t mr = lci::register_memory(recv_buffer, msg_size);
  lci::rmr_t rmr = lci::get_rmr(mr);

  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    KEEP_RETRY(status, lci::post_put_x(rank, send_buffer, msg_size, scq, 0, rmr)
                           .tag(tag)
                           .remote_comp(rcomp)());

    if (status.is_posted()) {
      KEEP_RETRY(status, lci::cq_pop(scq));
    }

    KEEP_RETRY(status, lci::cq_pop(rcq));
    util::check_buffer(recv_buffer, msg_size, 'a');
  }

  lci::deregister_memory(&mr);
  free(send_buffer);
  free(recv_buffer);
  lci::free_comp(&scq);
  lci::free_comp(&rcq);
}

void test_putImm_common(int nthreads, int nmsgs, size_t msg_size)
{
  fprintf(stderr, "test_putImm_common: nthreads=%d nmsgs=%d msg_size=%ld\n",
          nthreads, nmsgs, msg_size);
  util::spawn_threads(nthreads, test_putImm_worker_fn, nmsgs, msg_size);
}

TEST(COMM_PUTIMM, putImm)
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
      test_putImm_common(nthread, nmsgs, msg_size);
    }
  }

  lci::g_runtime_fina();
}

}  // namespace test_comm_putImm