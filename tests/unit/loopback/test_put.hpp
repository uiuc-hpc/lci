// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

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
      test_put_common(nthread, nmsgs, msg_size);
    }
  }

  lci::g_runtime_fina();
}

TEST(COMM_PUT, put_buffers_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t cq = lci::alloc_cq();
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
      status = lci::post_put_x(rank, nullptr, 0, cq, 0, lci::RMR_NULL)
                   .buffers(send_buffers)
                   .rbuffers(rbuffers)
                   .comp_semantic(lci::comp_semantic_t::network)();
      lci::progress();
    } while (status.is_retry());

    if (status.is_posted()) {
      // poll cq
      do {
        lci::progress();
        status = lci::cq_pop(cq);
      } while (status.is_retry());
    }
    // verify the result
    lci::buffers_t ret_buffers = status.get_buffers();
    ASSERT_EQ(ret_buffers.size(), send_buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, send_buffers[i].size);
      ASSERT_EQ(ret_buffers[i].base, send_buffers[i].base);
    }
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
  lci::free_comp(&cq);
  lci::g_runtime_fina();
}

void test_put_buffers_mt(int id, int nmsgs)
{
  int rank = lci::get_rank_me();
  // local cq
  lci::comp_t cq = lci::alloc_cq();
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
      status = lci::post_put_x(rank, nullptr, 0, cq, 0, lci::RMR_NULL)
                   .buffers(send_buffers)
                   .rbuffers(rbuffers)
                   .comp_semantic(lci::comp_semantic_t::network)();
      lci::progress();
    } while (status.is_retry());

    if (status.is_posted()) {
      // poll cq
      do {
        lci::progress();
        status = lci::cq_pop(cq);
      } while (status.is_retry());
    }
    // verify the result
    lci::buffers_t ret_buffers = status.get_buffers();
    ASSERT_EQ(ret_buffers.size(), send_buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, send_buffers[i].size);
      ASSERT_EQ(ret_buffers[i].base, send_buffers[i].base);
    }
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
  lci::free_comp(&cq);
}

TEST(COMM_PUT, put_buffers_mt)
{
  lci::g_runtime_init();

  const int nmsgs = util::NITERS_SMALL;
  const int nthreads = util::NTHREADS;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_put_buffers_mt, i, nmsgs / nthreads);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::g_runtime_fina();
}

}  // namespace test_comm_put