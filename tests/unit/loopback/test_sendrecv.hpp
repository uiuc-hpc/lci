// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace test_comm_sendrecv
{
TEST(COMM_SENDRECV, sendrecv_bcopy_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    uint64_t data = 0xdeadbeef;
    lci::status_t status;
    do {
      status = lci::post_recv(rank, &data, sizeof(data), 0, rcq);
      lci::progress();
    } while (status.error.is_retry());

    do {
      status = lci::post_send(rank, &data, sizeof(data), 0, scq);
      lci::progress();
    } while (status.error.is_retry());

    bool send_done = false, recv_done = false;
    if (status.error.is_ok()) {
      send_done = true;
    }

    while (!send_done || !recv_done) {
      lci::progress();
      if (!send_done) {
        status = lci::cq_pop(scq);
        if (status.error.is_ok()) {
          ASSERT_EQ(status.data.get_scalar<uint64_t>(), data);
          send_done = true;
        }
      }
      if (!recv_done) {
        status = lci::cq_pop(rcq);
        if (status.error.is_ok()) {
          ASSERT_EQ(status.data.get_scalar<uint64_t>(), data);
          recv_done = true;
        }
      }
    }
  }

  lci::free_comp(&rcq);
  lci::free_comp(&scq);
  lci::g_runtime_fina();
}

void test_sendrecv_bcopy_mt(int id, int nmsgs, uint64_t* p_data)
{
  int rank = lci::get_rank();
  lci::tag_t tag = id;
  // local cq
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();

  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_recv(rank, p_data, sizeof(uint64_t), id, rcq);
      lci::progress();
    } while (status.error.is_retry());

    do {
      status = lci::post_send(rank, p_data, sizeof(uint64_t), id, scq);
      lci::progress();
    } while (status.error.is_retry());

    bool send_done = false, recv_done = false;
    if (status.error.is_ok()) {
      send_done = true;
    }

    while (!send_done || !recv_done) {
      lci::progress();
      if (!send_done) {
        status = lci::cq_pop(scq);
        if (status.error.is_ok()) {
          ASSERT_EQ(status.data.get_scalar<uint64_t>(), *p_data);
          send_done = true;
        }
      }
      if (!recv_done) {
        status = lci::cq_pop(rcq);
        if (status.error.is_ok()) {
          ASSERT_EQ(status.data.get_scalar<uint64_t>(), *p_data);
          recv_done = true;
        }
      }
    }
  }

  lci::free_comp(&rcq);
  lci::free_comp(&scq);
}

TEST(COMM_SENDRECV, sendrecv_bcopy_mt)
{
  lci::g_runtime_init();

  const int nmsgs = 20000;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);

  uint64_t data = 0xdeadbeef;

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_sendrecv_bcopy_mt, i, nmsgs / nthreads, &data);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::g_runtime_fina();
}

TEST(COMM_SENDRECV, sendrecv_zcopy_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  // loopback message
  size_t msg_size = lci::get_max_bcopy_size() * 2;
  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  util::write_buffer(send_buffer, msg_size, 'a');

  for (int i = 0; i < nmsgs; i++) {
    uint64_t data = 0xdeadbeef;
    lci::status_t status;
    do {
      status = lci::post_recv(rank, recv_buffer, msg_size, i, rcq);
      lci::progress();
    } while (status.error.is_retry());
    do {
      status = lci::post_send(rank, send_buffer, msg_size, i, scq);
      lci::progress();
    } while (status.error.is_retry());

    if (status.error.is_posted()) {
      // poll send cq
      do {
        lci::progress();
        status = lci::cq_pop(scq);
      } while (status.error.is_retry());
    }
    // poll recv cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.error.is_retry());
    util::check_buffer(recv_buffer, msg_size, 'a');
  }

  free(send_buffer);
  free(recv_buffer);
  lci::free_comp(&rcq);
  lci::free_comp(&rcq);
  lci::g_runtime_fina();
}

void test_sendrecv_zcopy_mt(int id, int nmsgs)
{
  int rank = lci::get_rank();
  lci::tag_t tag = id;

  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  size_t msg_size = lci::get_max_bcopy_size() * 2;
  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  util::write_buffer(send_buffer, msg_size, 'a');

  for (int i = 0; i < nmsgs; i++) {
    uint64_t data = 0xdeadbeef;
    lci::status_t status;
    do {
      status = lci::post_recv(rank, recv_buffer, msg_size, id, rcq);
      lci::progress();
    } while (status.error.is_retry());
    do {
      status = lci::post_send(rank, send_buffer, msg_size, id, scq);
      lci::progress();
    } while (status.error.is_retry());

    if (status.error.is_posted()) {
      // poll send cq
      do {
        lci::progress();
        status = lci::cq_pop(scq);
      } while (status.error.is_retry());
    }
    // poll recv cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.error.is_retry());
    util::check_buffer(recv_buffer, msg_size, 'a');
  }

  free(send_buffer);
  free(recv_buffer);
  lci::free_comp(&rcq);
  lci::free_comp(&scq);
}

TEST(COMM_SENDRECV, sendrecv_zcopy_mt)
{
  lci::g_runtime_init();

  const int nmsgs = 20000;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_sendrecv_zcopy_mt, i, nmsgs / nthreads);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::g_runtime_fina();
}

TEST(COMM_SENDRECV, sendrecv_buffers_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  // prepare data
  uint64_t data0 = 0xdead;
  uint64_t data1 = 0xbeef;
  uint64_t data2 = 0xdeadbeef;
  lci::buffers_t send_buffers;
  send_buffers.push_back(lci::buffer_t(&data0, sizeof(data0)));
  send_buffers.push_back(lci::buffer_t(&data1, sizeof(data1)));
  send_buffers.push_back(lci::buffer_t(&data2, sizeof(data2)));
  uint64_t data3 = 0;
  uint64_t data4 = 0;
  uint64_t data5 = 0;
  lci::buffers_t recv_buffers;
  recv_buffers.push_back(lci::buffer_t(&data3, sizeof(data3)));
  recv_buffers.push_back(lci::buffer_t(&data4, sizeof(data4)));
  recv_buffers.push_back(lci::buffer_t(&data5, sizeof(data5)));
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status =
          lci::post_recv_x(rank, nullptr, 0, i, rcq).buffers(recv_buffers)();
      lci::progress();
    } while (status.error.is_retry());
    do {
      status =
          lci::post_send_x(rank, nullptr, 0, i, scq).buffers(send_buffers)();
      lci::progress();
    } while (status.error.is_retry());
    if (status.error.is_posted()) {
      // poll send cq
      do {
        lci::progress();
        status = lci::cq_pop(scq);
      } while (status.error.is_retry());
    }
    lci::buffers_t ret_buffers = status.data.get_buffers();
    ASSERT_EQ(ret_buffers.size(), send_buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, send_buffers[i].size);
      ASSERT_EQ(ret_buffers[i].base, send_buffers[i].base);
    }
    // poll recv cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.error.is_retry());
    ret_buffers = status.data.get_buffers();
    ASSERT_EQ(ret_buffers.size(), recv_buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, recv_buffers[i].size);
      ASSERT_EQ(*(uint64_t*)ret_buffers[i].base,
                *(uint64_t*)recv_buffers[i].base);
    }
  }

  lci::free_comp(&scq);
  lci::free_comp(&rcq);
  lci::g_runtime_fina();
}

void test_sendrecv_buffers_mt(int id, int nmsgs)
{
  int rank = lci::get_rank();
  // local cq
  lci::comp_t scq = lci::alloc_cq();
  lci::comp_t rcq = lci::alloc_cq();
  // prepare data
  uint64_t data0 = 0xdead;
  uint64_t data1 = 0xbeef;
  uint64_t data2 = 0xdeadbeef;
  lci::buffers_t send_buffers;
  send_buffers.push_back(lci::buffer_t(&data0, sizeof(data0)));
  send_buffers.push_back(lci::buffer_t(&data1, sizeof(data1)));
  send_buffers.push_back(lci::buffer_t(&data2, sizeof(data2)));
  uint64_t data3 = 0;
  uint64_t data4 = 0;
  uint64_t data5 = 0;
  lci::buffers_t recv_buffers;
  recv_buffers.push_back(lci::buffer_t(&data3, sizeof(data3)));
  recv_buffers.push_back(lci::buffer_t(&data4, sizeof(data4)));
  recv_buffers.push_back(lci::buffer_t(&data5, sizeof(data5)));
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status =
          lci::post_recv_x(rank, nullptr, 0, id, rcq).buffers(recv_buffers)();
      lci::progress();
    } while (status.error.is_retry());
    do {
      status =
          lci::post_send_x(rank, nullptr, 0, id, scq).buffers(send_buffers)();
      lci::progress();
    } while (status.error.is_retry());
    if (status.error.is_posted()) {
      // poll send cq
      do {
        lci::progress();
        status = lci::cq_pop(scq);
      } while (status.error.is_retry());
    }
    lci::buffers_t ret_buffers = status.data.get_buffers();
    ASSERT_EQ(ret_buffers.size(), send_buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, send_buffers[i].size);
      ASSERT_EQ(ret_buffers[i].base, send_buffers[i].base);
    }
    // poll recv cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.error.is_retry());
    ret_buffers = status.data.get_buffers();
    ASSERT_EQ(ret_buffers.size(), recv_buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, recv_buffers[i].size);
      ASSERT_EQ(*(uint64_t*)ret_buffers[i].base,
                *(uint64_t*)recv_buffers[i].base);
    }
  }

  lci::free_comp(&scq);
  lci::free_comp(&rcq);
}

TEST(COMM_SENDRECV, sendrecv_buffers_mt)
{
  lci::g_runtime_init();

  const int nmsgs = 4992;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);

  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_sendrecv_buffers_mt, i, nmsgs / nthreads);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::g_runtime_fina();
}

}  // namespace test_comm_sendrecv