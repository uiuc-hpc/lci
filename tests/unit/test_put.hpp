// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

#include "lci_internal.hpp"

namespace test_comm_put
{
TEST(COMM_PUT, put_bcopy_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t cq = lci::alloc_cq();
  // prepare buffer
  size_t msg_size = lci::get_max_bcopy_size();
  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  util::write_buffer(send_buffer, msg_size, 'a');
  // register recv buffer
  lci::mr_t mr = lci::register_memory(recv_buffer, msg_size);
  lci::rkey_t rkey = lci::get_rkey(mr);

  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_put_x(rank, send_buffer, msg_size, cq,
                               reinterpret_cast<uintptr_t>(recv_buffer), rkey)
                   .comp_semantic(lci::comp_semantic_t::network)();
      lci::progress();
    } while (status.error.is_retry());

    if (status.error.is_posted()) {
      do {
        lci::progress();
        status = lci::cq_pop(cq);
      } while (status.error.is_retry());
    }

    util::check_buffer(recv_buffer, msg_size, 'a');
  }
  // clean up
  lci::deregister_memory(&mr);
  free(send_buffer);
  free(recv_buffer);
  lci::free_comp(&cq);
  lci::g_runtime_fina();
}

void test_put_bcopy_mt(int id, int nmsgs, uint64_t* p_data)
{
  int rank = lci::get_rank();
  lci::tag_t tag = id;
  // local cq
  lci::comp_t cq = lci::alloc_cq();
  // prepare buffer
  size_t msg_size = lci::get_max_bcopy_size();
  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  util::write_buffer(send_buffer, msg_size, 'a');
  // register recv buffer
  lci::mr_t mr = lci::register_memory(recv_buffer, msg_size);
  lci::rkey_t rkey = lci::get_rkey(mr);

  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_put_x(rank, send_buffer, msg_size, cq,
                               reinterpret_cast<uintptr_t>(recv_buffer), rkey)
                   .comp_semantic(lci::comp_semantic_t::network)();
      lci::progress();
    } while (status.error.is_retry());

    if (status.error.is_posted()) {
      do {
        lci::progress();
        status = lci::cq_pop(cq);
      } while (status.error.is_retry());
    }

    util::check_buffer(recv_buffer, msg_size, 'a');
  }
  // clean up
  lci::deregister_memory(&mr);
  free(send_buffer);
  free(recv_buffer);
  lci::free_comp(&cq);
}

TEST(COMM_PUT, put_bcopy_mt)
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
    std::thread t(test_put_bcopy_mt, i, nmsgs / nthreads, &data);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::g_runtime_fina();
}

TEST(COMM_PUT, put_zcopy_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
  ASSERT_EQ(rank, 0);
  ASSERT_EQ(nranks, 1);
  // local cq
  lci::comp_t cq = lci::alloc_cq();
  // loopback message
  size_t msg_size = lci::get_max_bcopy_size() * 2;
  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  util::write_buffer(send_buffer, msg_size, 'a');
  // register recv buffer
  lci::mr_t mr = lci::register_memory(recv_buffer, msg_size);
  lci::rkey_t rkey = lci::get_rkey(mr);
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_put_x(rank, send_buffer, msg_size, cq,
                               reinterpret_cast<uintptr_t>(recv_buffer), rkey)
                   .comp_semantic(lci::comp_semantic_t::network)();
      lci::progress();
    } while (status.error.is_retry());

    if (status.error.is_posted()) {
      do {
        lci::progress();
        status = lci::cq_pop(cq);
      } while (status.error.is_retry());
    }

    util::check_buffer(recv_buffer, msg_size, 'a');
  }
  // clean up
  lci::deregister_memory(&mr);
  free(send_buffer);
  free(recv_buffer);
  lci::free_comp(&cq);
  lci::g_runtime_fina();
}

void test_put_zcopy_mt(int id, int nmsgs)
{
  int rank = lci::get_rank();
  lci::tag_t tag = id;

  lci::comp_t cq = lci::alloc_cq();
  size_t msg_size = lci::get_max_bcopy_size() * 2;
  void* send_buffer = malloc(msg_size);
  void* recv_buffer = malloc(msg_size);
  util::write_buffer(send_buffer, msg_size, 'a');
  // register recv buffer
  lci::mr_t mr = lci::register_memory(recv_buffer, msg_size);
  lci::rkey_t rkey = lci::get_rkey(mr);
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_put_x(rank, send_buffer, msg_size, cq,
                               reinterpret_cast<uintptr_t>(recv_buffer), rkey)
                   .comp_semantic(lci::comp_semantic_t::network)();
      lci::progress();
    } while (status.error.is_retry());

    if (status.error.is_posted()) {
      do {
        lci::progress();
        status = lci::cq_pop(cq);
      } while (status.error.is_retry());
    }

    util::check_buffer(recv_buffer, msg_size, 'a');
  }
  // clean up
  lci::deregister_memory(&mr);
  free(send_buffer);
  free(recv_buffer);
  lci::free_comp(&cq);
}

TEST(COMM_PUT, put_zcopy_mt)
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
    std::thread t(test_put_zcopy_mt, i, nmsgs / nthreads);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }

  lci::g_runtime_fina();
}

TEST(COMM_PUT, put_buffers_st)
{
  lci::g_runtime_init();

  const int nmsgs = 1000;
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
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
    rbuffers[i].base = reinterpret_cast<uintptr_t>(recv_buffers[i].base);
    rbuffers[i].rkey = lci::get_rkey(recv_buffers[i].mr);
  }
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_put_x(rank, nullptr, 0, cq, 0, 0)
                   .buffers(send_buffers)
                   .rbuffers(rbuffers)
                   .comp_semantic(lci::comp_semantic_t::network)();
      lci::progress();
    } while (status.error.is_retry());

    if (status.error.is_posted()) {
      // poll cq
      do {
        lci::progress();
        status = lci::cq_pop(cq);
      } while (status.error.is_retry());
    }
    // verify the result
    lci::buffers_t ret_buffers = status.data.get_buffers();
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
  int rank = lci::get_rank();
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
    rbuffers[i].base = reinterpret_cast<uintptr_t>(recv_buffers[i].base);
    rbuffers[i].rkey = lci::get_rkey(recv_buffers[i].mr);
  }
  // loopback message
  for (int i = 0; i < nmsgs; i++) {
    lci::status_t status;
    do {
      status = lci::post_put_x(rank, nullptr, 0, cq, 0, 0)
                   .buffers(send_buffers)
                   .rbuffers(rbuffers)
                   .comp_semantic(lci::comp_semantic_t::network)();
      lci::progress();
    } while (status.error.is_retry());

    if (status.error.is_posted()) {
      // poll cq
      do {
        lci::progress();
        status = lci::cq_pop(cq);
      } while (status.error.is_retry());
    }
    // verify the result
    lci::buffers_t ret_buffers = status.data.get_buffers();
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

  const int nmsgs = 4992;
  const int nthreads = 16;
  ASSERT_EQ(nmsgs % nthreads, 0);
  int rank = lci::get_rank();
  int nranks = lci::get_nranks();
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