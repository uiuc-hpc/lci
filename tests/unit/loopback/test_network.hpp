// Copyright (c) 2025 The LCI Project Authors
// SPDX-License-Identifier: MIT

namespace test_network
{
TEST(NETWORK, reg_mem)
{
  lci::g_runtime_init();
  const int size = 1024;
  void* address = malloc(size);
  lci::mr_t mr = lci::register_memory(address, size);
  lci::deregister_memory(&mr);
  lci::g_runtime_fina();
}

TEST(NETWORK, poll_cq)
{
  lci::g_runtime_init();
  lci::net_status_t statuses[LCI_BACKEND_MAX_POLLS];
  size_t ret = lci::net_poll_cq(LCI_BACKEND_MAX_POLLS, statuses);
  ASSERT_EQ(ret, 0);
  lci::g_runtime_fina();
}

TEST(NETWORK, loopback)
{
  lci::g_runtime_init();
  const int size = 1024;
  void* address = malloc(size);
  lci::mr_t mr = lci::register_memory(address, size);
  while (lci::net_post_recv(address, size, mr).is_retry()) continue;
  while (lci::net_post_send(0, address, size, mr).is_retry()) continue;
  size_t total = 0;
  while (total < 2) {
    lci::net_status_t statuses[LCI_BACKEND_MAX_POLLS];
    size_t ret = lci::net_poll_cq(LCI_BACKEND_MAX_POLLS, statuses);
    total += ret;
  }
  lci::deregister_memory(&mr);
  lci::g_runtime_fina();
}

void test_loopback_mt(int id, int nmsgs, int size, void* address, lci::mr_t mr)
{
  for (int i = 0; i < nmsgs; ++i) {
    std::atomic<int> count(0);
    while (lci::net_post_recv_x(address, size, mr)
               .user_context(&count)()
               .is_retry())
      continue;
    while (lci::net_post_send_x(0, address, size, mr)
               .user_context(&count)()
               .is_retry())
      continue;
    while (count.load() < 2) {
      lci::net_status_t status;
      size_t ret = lci::net_poll_cq(1, &status);
      if (ret > 0) {
        ASSERT_EQ(ret, 1);
        auto* p = (std::atomic<int>*)status.user_context;
        p->fetch_add(1);
      }
    }
    ASSERT_EQ(count.load(), 2);
  }
}

TEST(NETWORK, loopback_mt)
{
  lci::g_runtime_init_x().alloc_default_packet_pool(false)();
  const int size = 1024;
  const int nthreads = 16;
  const int nmsgs = 10000;
  ASSERT_EQ(nmsgs % nthreads, 0);
  void* address = malloc(size);
  lci::mr_t mr = lci::register_memory(address, size);
  std::vector<std::thread> threads;
  for (int i = 0; i < nthreads; i++) {
    std::thread t(test_loopback_mt, i, nmsgs / nthreads, size, address, mr);
    threads.push_back(std::move(t));
  }
  for (auto& t : threads) {
    t.join();
  }
  lci::deregister_memory(&mr);
  lci::g_runtime_fina();
}

TEST(NETWORK, loopback_put)
{
  lci::g_runtime_init();
  const int size = 1024;
  void* send_address = malloc(size);
  void* recv_address = malloc(size);
  lci::mr_t send_mr = lci::register_memory(send_address, size);
  lci::mr_t recv_mr = lci::register_memory(recv_address, size);
  lci::rkey_t rkey = lci::get_rkey(recv_mr);
  while (lci::net_post_put(0, send_address, size, send_mr,
                           reinterpret_cast<uintptr_t>(recv_address), 0, rkey)
             .is_retry())
    continue;
  size_t total = 0;
  while (total < 1) {
    lci::net_status_t status;
    size_t ret = lci::net_poll_cq(1, &status);
    total += ret;
  }
  lci::deregister_memory(&send_mr);
  lci::deregister_memory(&recv_mr);
  lci::g_runtime_fina();
}

TEST(NETWORK, loopback_get)
{
  lci::g_runtime_init();
  const int size = 1024;
  void* send_address = malloc(size);
  void* recv_address = malloc(size);
  lci::mr_t send_mr = lci::register_memory(send_address, size);
  lci::mr_t recv_mr = lci::register_memory(recv_address, size);
  lci::rkey_t rkey = lci::get_rkey(send_mr);
  while (lci::net_post_get(0, recv_address, size, recv_mr,
                           reinterpret_cast<uintptr_t>(send_address), 0, rkey)
             .is_retry())
    continue;
  size_t total = 0;
  while (total < 1) {
    lci::net_status_t status;
    size_t ret = lci::net_poll_cq(1, &status);
    total += ret;
  }
  lci::deregister_memory(&send_mr);
  lci::deregister_memory(&recv_mr);
  lci::g_runtime_fina();
}

}  // namespace test_network