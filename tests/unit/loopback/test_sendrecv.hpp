// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

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

TEST(COMM_SENDRECV, sendrecv_buffers_st)
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
    } while (status.is_retry());
    do {
      status =
          lci::post_send_x(rank, nullptr, 0, i, scq).buffers(send_buffers)();
      lci::progress();
    } while (status.is_retry());
    if (status.is_posted()) {
      // poll send cq
      do {
        lci::progress();
        status = lci::cq_pop(scq);
      } while (status.is_retry());
    }
    lci::buffers_t ret_buffers = status.get_buffers();
    ASSERT_EQ(ret_buffers.size(), send_buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, send_buffers[i].size);
      ASSERT_EQ(ret_buffers[i].base, send_buffers[i].base);
    }
    // poll recv cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.is_retry());
    ret_buffers = status.get_buffers();
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
  int rank = lci::get_rank_me();
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
    } while (status.is_retry());
    do {
      status =
          lci::post_send_x(rank, nullptr, 0, id, scq).buffers(send_buffers)();
      lci::progress();
    } while (status.is_retry());
    if (status.is_posted()) {
      // poll send cq
      do {
        lci::progress();
        status = lci::cq_pop(scq);
      } while (status.is_retry());
    }
    lci::buffers_t ret_buffers = status.get_buffers();
    ASSERT_EQ(ret_buffers.size(), send_buffers.size());
    for (size_t i = 0; i < ret_buffers.size(); i++) {
      ASSERT_EQ(ret_buffers[i].size, send_buffers[i].size);
      ASSERT_EQ(ret_buffers[i].base, send_buffers[i].base);
    }
    // poll recv cq
    do {
      status = lci::cq_pop(rcq);
      lci::progress();
    } while (status.is_retry());
    ret_buffers = status.get_buffers();
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

  const int nmsgs = util::NITERS_SMALL;
  const int nthreads = util::NTHREADS;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank_me();
  int nranks = lci::get_rank_n();
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