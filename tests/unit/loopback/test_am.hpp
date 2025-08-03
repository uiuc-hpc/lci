// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace test_comm_am
{
void test_am_worker_fn(int thread_id, int nmsgs, size_t msg_size)
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
    KEEP_RETRY(
        status,
        lci::post_am_x(rank, send_buffer, msg_size, lcq, rcomp).tag(tag)());

    // poll cqs
    bool lcq_done = status.is_done();
    bool rcq_done = false;
    while (!lcq_done || !rcq_done) {
      lci::progress();
      if (!lcq_done) {
        status = lci::cq_pop(lcq);
        if (status.is_done()) {
          lci::buffer_t buffer = status.get_buffer();
          ASSERT_EQ(buffer.base, send_buffer);
          ASSERT_EQ(buffer.size, msg_size);
          lcq_done = true;
        }
      }
      if (!rcq_done) {
        status = lci::cq_pop(rcq);
        if (status.is_done()) {
          lci::buffer_t buffer = status.get_buffer();
          ASSERT_EQ(buffer.size, msg_size);
          util::check_buffer(buffer.base, msg_size, 'a');
          free(buffer.base);
          rcq_done = true;
        }
      }
    }
  }

  free(send_buffer);
  lci::free_comp(&lcq);
  lci::free_comp(&rcq);
}

void test_am_common(int nthreads, int nmsgs, size_t msg_size)
{
  fprintf(stderr, "test_am_common: nthreads=%d nmsgs=%d msg_size=%ld\n",
          nthreads, nmsgs, msg_size);
  util::spawn_threads(nthreads, test_am_worker_fn, nmsgs, msg_size);
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
      test_am_common(nthread, nmsgs, msg_size);
    }
  }

  lci::g_runtime_fina();
}

TEST(COMM_AM, am_buffers_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t lcq = lci::alloc_cq();

  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);
  // prepare data
  uint64_t data0 = 0xdead;
  uint64_t data1 = 0xbeef;
  uint64_t data2 = 0xdeadbeef;
  lci::buffers_t buffers;
  buffers.push_back(lci::buffer_t(&data0, sizeof(data0)));
  buffers.push_back(lci::buffer_t(&data1, sizeof(data1)));
  buffers.push_back(lci::buffer_t(&data2, sizeof(data2)));
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_am_x(rank, nullptr, 0, lcq, rcomp).buffers(buffers)();
      lci::progress();
    } while (status.is_retry());
    if (status.is_posted()) {
      // poll local cq
      do {
        lci::progress();
        status = lci::cq_pop(lcq);
      } while (status.is_retry());
    }
    lci::buffers_t ret_buffers = status.get_buffers();
    ASSERT_EQ(ret_buffers.size(), buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, buffers[i].size);
      ASSERT_EQ(ret_buffers[i].base, buffers[i].base);
    }
    // poll remote cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.is_retry());
    ret_buffers = status.get_buffers();
    ASSERT_EQ(ret_buffers.size(), buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, buffers[i].size);
      ASSERT_EQ(*(uint64_t*)ret_buffers[i].base, *(uint64_t*)buffers[i].base);
      free(ret_buffers[i].base);
    }
  }

  lci::free_comp(&lcq);
  lci::free_comp(&rcq);
  lci::g_runtime_fina();
}

void test_am_buffers_mt(int id, int nmsgs, lci::comp_t lcq, lci::comp_t rcq,
                        lci::rcomp_t rcomp, const lci::buffers_t& buffers)
{
  int rank = lci::get_rank_me();
  lci::tag_t tag = id;

  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_am_x(rank, nullptr, 0, lcq, rcomp)
                   .tag(tag)
                   .buffers(buffers)();
      lci::progress();
    } while (status.is_retry());
    // poll cqs
    bool lcq_done = status.is_done();
    bool rcq_done = false;
    while (!lcq_done || !rcq_done) {
      lci::progress();
      if (!lcq_done) {
        status = lci::cq_pop(lcq);
        if (status.is_done()) {
          lci::buffers_t ret_buffers = status.get_buffers();
          ASSERT_EQ(ret_buffers.size(), buffers.size());
          for (size_t i = 0; i < ret_buffers.size(); i++) {
            ASSERT_EQ(ret_buffers[i].size, buffers[i].size);
            ASSERT_EQ(ret_buffers[i].base, buffers[i].base);
          }
          lcq_done = true;
        }
      }
      if (!rcq_done) {
        status = lci::cq_pop(rcq);
        if (status.is_done()) {
          lci::buffers_t ret_buffers = status.get_buffers();
          ASSERT_EQ(ret_buffers.size(), buffers.size());
          for (size_t i = 0; i < ret_buffers.size(); i++) {
            ASSERT_EQ(ret_buffers[i].size, buffers[i].size);
            ASSERT_EQ(*(uint64_t*)ret_buffers[i].base,
                      *(uint64_t*)buffers[i].base);
            free(ret_buffers[i].base);
          }
          rcq_done = true;
        }
      }
    }
  }
}

TEST(COMM_AM, am_buffers_mt)
{
  lci::g_runtime_init();

  const int nmsgs = util::NITERS_SMALL;
  const int nthreads = util::NTHREADS;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  lci::comp_t lcq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);
  // prepare data
  uint64_t data0 = 0xdead;
  uint64_t data1 = 0xbeef;
  uint64_t data2 = 0xdeadbeef;
  lci::buffers_t buffers;
  buffers.push_back(lci::buffer_t(&data0, sizeof(data0)));
  buffers.push_back(lci::buffer_t(&data1, sizeof(data1)));
  buffers.push_back(lci::buffer_t(&data2, sizeof(data2)));

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_am_buffers_mt, i, nmsgs / nthreads, lcq, rcq, rcomp,
                  std::ref(buffers));
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::free_comp(&lcq);
  lci::free_comp(&rcq);
  lci::g_runtime_fina();
}

}  // namespace test_comm_am