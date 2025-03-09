// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace test_comm_am_bq
{
TEST(BACKLOG_QUEUE, am_bcopy_st_bq)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t lcq = lci::alloc_cq();

  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    uint64_t data = 0xdeadbeef;
    lci::status_t status;
    status = lci::post_am_x(rank, &data, sizeof(data), lcq, rcomp)
                 .allow_retry(false)();
    ASSERT_EQ(status.error.is_retry(), false);
    if (status.error.is_posted()) {
      // poll local cq
      do {
        lci::progress();
        status = lci::cq_pop(lcq);
      } while (status.error.is_retry());
    }
    ASSERT_EQ(status.data.get_scalar<uint64_t>(), data);
    // poll remote cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.error.is_retry());
    ASSERT_EQ(status.data.get_scalar<uint64_t>(), data);
  }

  lci::free_comp(&lcq);
  lci::free_comp(&rcq);
  lci::g_runtime_fina();
}

void test_am_bcopy_mt(int id, int nmsgs, lci::comp_t lcq, lci::comp_t rcq,
                      lci::rcomp_t rcomp, uint64_t* p_data)
{
  int rank = lci::get_rank();
  lci::tag_t tag = id;

  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    status = lci::post_am_x(rank, p_data, sizeof(uint64_t), lcq, rcomp)
                 .tag(tag)
                 .allow_retry(false)();
    ASSERT_EQ(status.error.is_retry(), false);
    // poll cqs
    bool lcq_done = status.error.is_ok();
    bool rcq_done = false;
    while (!lcq_done || !rcq_done) {
      lci::progress();
      if (!lcq_done) {
        status = lci::cq_pop(lcq);
        if (status.error.is_ok()) {
          lci::buffer_t buffer = status.data.get_buffer();
          ASSERT_EQ(buffer.base, p_data);
          ASSERT_EQ(*(uint64_t*)buffer.base, *p_data);
          lcq_done = true;
        }
      }
      if (!rcq_done) {
        status = lci::cq_pop(rcq);
        if (status.error.is_ok()) {
          ASSERT_EQ(status.data.get_scalar<uint64_t>(), *p_data);
          rcq_done = true;
        }
      }
    }
  }
}

TEST(BACKLOG_QUEUE, am_bcopy_mt_bq)
{
  lci::g_runtime_init();

  const int nmsgs = 20000;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  lci::comp_t lcq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);

  uint64_t data = 0xdeadbeef;

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_am_bcopy_mt, i, nmsgs / nthreads, lcq, rcq, rcomp,
                  &data);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::free_comp(&lcq);
  lci::free_comp(&rcq);
  lci::g_runtime_fina();
}

TEST(BACKLOG_QUEUE, am_zcopy_st_bq)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t lcq = lci::alloc_cq();

  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    uint64_t data = 0xdeadbeef;
    lci::status_t status;
    status = lci::post_am_x(rank, &data, sizeof(data), lcq, rcomp)
                 .force_zcopy(true)
                 .allow_retry(false)();
    ASSERT_EQ(status.error.is_retry(), false);
    if (status.error.is_posted()) {
      // poll local cq
      do {
        lci::progress();
        status = lci::cq_pop(lcq);
      } while (status.error.is_retry());
    }
    ASSERT_EQ(status.data.get_scalar<uint64_t>(), data);
    // poll remote cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.error.is_retry());
    ASSERT_EQ(status.data.get_scalar<uint64_t>(), data);
  }

  lci::free_comp(&lcq);
  lci::free_comp(&rcq);
  lci::g_runtime_fina();
}

void test_am_zcopy_mt(int id, int nmsgs, lci::comp_t lcq, lci::comp_t rcq,
                      lci::rcomp_t rcomp, uint64_t* p_data)
{
  int rank = lci::get_rank();
  lci::tag_t tag = id;

  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    status = lci::post_am_x(rank, p_data, sizeof(uint64_t), lcq, rcomp)
                 .tag(tag)
                 .force_zcopy(true)
                 .allow_retry(false)();
    ASSERT_EQ(status.error.is_retry(), false);
    // poll cqs
    bool lcq_done = status.error.is_ok();
    bool rcq_done = false;
    while (!lcq_done || !rcq_done) {
      lci::progress();
      if (!lcq_done) {
        status = lci::cq_pop(lcq);
        if (status.error.is_ok()) {
          lci::buffer_t buffer = status.data.get_buffer();
          ASSERT_EQ(buffer.base, p_data);
          ASSERT_EQ(*(uint64_t*)buffer.base, *p_data);
          lcq_done = true;
        }
      }
      if (!rcq_done) {
        status = lci::cq_pop(rcq);
        if (status.error.is_ok()) {
          ASSERT_EQ(status.data.get_scalar<uint64_t>(), *p_data);
          rcq_done = true;
        }
      }
    }
  }
}

TEST(BACKLOG_QUEUE, am_zcopy_mt_bq)
{
  lci::g_runtime_init();

  const int nmsgs = 20000;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  lci::comp_t lcq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  lci::rcomp_t rcomp = lci::register_rcomp(rcq);

  uint64_t data = 0xdeadbeef;

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_am_zcopy_mt, i, nmsgs / nthreads, lcq, rcq, rcomp,
                  &data);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::free_comp(&lcq);
  lci::free_comp(&rcq);
  lci::g_runtime_fina();
}

TEST(BACKLOG_QUEUE, am_buffers_st_bq)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
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
    status = lci::post_am_x(rank, nullptr, 0, lcq, rcomp)
                 .buffers(buffers)
                 .allow_retry(false)();
    if (status.error.is_posted()) {
      // poll local cq
      do {
        lci::progress();
        status = lci::cq_pop(lcq);
      } while (status.error.is_retry());
    }
    lci::buffers_t ret_buffers = status.data.get_buffers();
    ASSERT_EQ(ret_buffers.size(), buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, buffers[i].size);
      ASSERT_EQ(ret_buffers[i].base, buffers[i].base);
    }
    // poll remote cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.error.is_retry());
    ret_buffers = status.data.get_buffers();
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
  int rank = lci::get_rank();
  lci::tag_t tag = id;

  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    status = lci::post_am_x(rank, nullptr, 0, lcq, rcomp)
                 .tag(tag)
                 .buffers(buffers)
                 .allow_retry(false)();
    // poll cqs
    bool lcq_done = status.error.is_ok();
    bool rcq_done = false;
    while (!lcq_done || !rcq_done) {
      lci::progress();
      if (!lcq_done) {
        status = lci::cq_pop(lcq);
        if (status.error.is_ok()) {
          lci::buffers_t ret_buffers = status.data.get_buffers();
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
        if (status.error.is_ok()) {
          lci::buffers_t ret_buffers = status.data.get_buffers();
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

TEST(BACKLOG_QUEUE, am_buffers_mt)
{
  lci::g_runtime_init();

  const int nmsgs = 4992;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
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

}  // namespace test_comm_am_bq