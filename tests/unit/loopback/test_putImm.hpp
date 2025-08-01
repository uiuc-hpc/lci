// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

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

  const int nmsgs_total = 1000;
  const size_t max_bcopy_size = lci::get_max_bcopy_size();
  std::vector<size_t> msg_sizes = {0, 8, max_bcopy_size, max_bcopy_size + 1,
                                   65536};
  std::vector<int> nthreads = {1, 4};

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

TEST(COMM_PUTIMM, putImm_buffers_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);
  // prepare data
  const int buffers_count = 3;
  size_t msg_size = lci::get_max_bcopy_size() * 2;
  lci::buffers_t send_buffers(buffers_count);
  lci::buffers_t recv_buffers(buffers_count);
  lci::rbuffers_t rbuffers(buffers_count);
  for (int i = 0; i < buffers_count; i++) {
    send_buffers[i].base = malloc(msg_size);
    send_buffers[i].size = msg_size;
    util::write_buffer(send_buffers[i].base, send_buffers[i].size, 'b');
    recv_buffers[i].base = malloc(msg_size);
    recv_buffers[i].size = msg_size;
    recv_buffers[i].mr = lci::register_memory(recv_buffers[i].base, msg_size);
    rbuffers[i].disp = 0;
    rbuffers[i].rmr = lci::get_rmr(recv_buffers[i].mr);
  }
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_put_x(rank, nullptr, 0, scq, 0, lci::RMR_NULL)
                   .buffers(send_buffers)
                   .rbuffers(rbuffers)
                   .tag(i)
                   .remote_comp(rcomp)();
      lci::progress();
    } while (status.is_retry());

    if (status.is_posted()) {
      // poll send cq
      do {
        lci::progress();
        status = lci::cq_pop(scq);
      } while (status.is_retry());
    }
    // poll receive cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.is_retry());
    // verify the result
    ASSERT_EQ(status.tag, i);
    for (auto& buffer : recv_buffers) {
      ASSERT_EQ(buffer.size, msg_size);
      util::check_buffer(buffer.base, buffer.size, 'b');
    }
  }
  // clean up
  for (int i = 0; i < buffers_count; i++) {
    lci::deregister_memory(&recv_buffers[i].mr);
    free(send_buffers[i].base);
    free(recv_buffers[i].base);
  }
  lci::free_comp(&scq);
  lci::free_comp(&rcq);
  lci::g_runtime_fina();
}

void test_putImm_buffers_mt(int id, int nmsgs, lci::rcomp_t rcomp_base)
{
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp_x(rcq).rcomp(rcomp_base + id)();
  // prepare data
  const int buffers_count = 3;
  size_t msg_size = lci::get_max_bcopy_size() * 2;
  lci::buffers_t send_buffers(buffers_count);
  lci::buffers_t recv_buffers(buffers_count);
  lci::rbuffers_t rbuffers(buffers_count);
  for (int i = 0; i < buffers_count; i++) {
    send_buffers[i].base = malloc(msg_size);
    send_buffers[i].size = msg_size;
    util::write_buffer(send_buffers[i].base, send_buffers[i].size, 'b');
    recv_buffers[i].base = malloc(msg_size);
    recv_buffers[i].size = msg_size;
    recv_buffers[i].mr = lci::register_memory(recv_buffers[i].base, msg_size);
    rbuffers[i].disp = 0;
    rbuffers[i].rmr = lci::get_rmr(recv_buffers[i].mr);
  }
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_put_x(rank, nullptr, 0, scq, 0, lci::RMR_NULL)
                   .buffers(send_buffers)
                   .rbuffers(rbuffers)
                   .tag(i)
                   .remote_comp(rcomp)();
      lci::progress();
    } while (status.is_retry());

    if (status.is_posted()) {
      // poll send cq
      do {
        lci::progress();
        status = lci::cq_pop(scq);
      } while (status.is_retry());
    }
    // poll receive cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.is_retry());
    // verify the result
    ASSERT_EQ(status.tag, i);
    for (auto& buffer : recv_buffers) {
      ASSERT_EQ(buffer.size, msg_size);
      util::check_buffer(buffer.base, buffer.size, 'b');
    }
  }
  // clean up
  for (int i = 0; i < buffers_count; i++) {
    lci::deregister_memory(&recv_buffers[i].mr);
    free(send_buffers[i].base);
    free(recv_buffers[i].base);
  }
  lci::free_comp(&scq);
  lci::free_comp(&rcq);
}

TEST(COMM_PUTIMM, putImm_buffers_mt)
{
  lci::g_runtime_init();

  const int nmsgs = util::NITERS_SMALL;
  const int nthreads = util::NTHREADS;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);

  lci::rcomp_t rcomp_base = lci::reserve_rcomps(nthreads);

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_putImm_buffers_mt, i, nmsgs / nthreads, rcomp_base);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::g_runtime_fina();
}

}  // namespace test_comm_putImm